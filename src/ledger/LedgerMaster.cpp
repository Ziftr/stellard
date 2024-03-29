#include "LedgerMaster.h"
#include "LegacyCLF.h"
#include "ripple_app/main/Application.h"
#include "ripple_basics/log/Log.h"
#include "ripple_basics/ripple_basics.h"
#include "ripple_app/main/LoadManager.h"
#include "LedgerEntry.h"

using namespace ripple; // needed for logging...

namespace stellar
{
    LedgerMaster::pointer gLedgerMaster;

    LedgerMaster::LedgerMaster() : mCurrentDB(getApp().getWorkingLedgerDB())
    {
        mCaughtUp = false;
        reset();
    }

    void LedgerMaster::reset()
    {
        mCurrentCLF = LegacyCLF::pointer(new LegacyCLF(this)); // change this to BucketList when we are ready
        mLastLedgerHash = uint256();
    }

    bool LedgerMaster::ensureSync(ripple::Ledger::pointer lastClosedLedger, bool checkLocal)
    {
        bool res = false;
        // first, make sure we're in sync with the world
        if (lastClosedLedger->getHash() != mLastLedgerHash)
        {
            if (checkLocal)
            {
                std::vector<uint256> needed = lastClosedLedger->getNeededAccountStateHashes(1, NULL);
                if (needed.size())
                {
                    // we're missing some nodes
                    return false;
                }
            }

            try
            {
                CanonicalLedgerForm::pointer newCLF, currentCLF(new LegacyCLF(this, lastClosedLedger));
                newCLF = catchUp(currentCLF);
                res = (newCLF != nullptr);
            }
            catch (...)
            {
                // problem applying to the database
                WriteLog(ripple::lsERROR, ripple::Ledger) << "database error";
            }
        }
        else
        { // already tracking proper ledger
            res = true;
        }

        return res;
    }

    void LedgerMaster::beginClosingLedger()
    {
        // ready to make changes
        mCurrentDB.beginTransaction();
        assert(mCurrentDB.getTransactionLevel() == 1); // should be top level transaction
    }

    bool  LedgerMaster::commitLedgerClose(ripple::Ledger::pointer ledger)
    {
        bool res = false;
        CanonicalLedgerForm::pointer newCLF;

        assert(ledger->getParentHash() == mLastLedgerHash); // should not happen

        try
        {
            CanonicalLedgerForm::pointer nl(new LegacyCLF(this, ledger));
            try
            {
                // only need to update ledger related fields as the account state is already in SQL
                updateDBFromLedger(nl);
                newCLF = nl;
            }
            catch (std::runtime_error const &)
            {
                WriteLog(ripple::lsERROR, ripple::Ledger) << "commitLedgerClose: could not update database";
            }

            if (newCLF != nullptr)
            {
                mCurrentDB.endTransaction(true);
                setLastClosedLedger(newCLF);
                res = true;
            }
            else
            {
                mCurrentDB.endTransaction(false);
            }
        }
        catch (...)
        {
            WriteLog(ripple::lsERROR, ripple::Ledger) << "commitLedgerClose: could not commit while updating the database";
        }
        return res;
    }

    void LedgerMaster::setLastClosedLedger(CanonicalLedgerForm::pointer ledger)
    {
        // should only be done outside of transactions, to guarantee state reflects what is on disk
        assert(mCurrentDB.getTransactionLevel() == 0);
        mCurrentCLF = ledger;
        mLastLedgerHash = ledger->getHash();
        WriteLog(ripple::lsINFO, ripple::Ledger) << "Store at " << mLastLedgerHash;
    }

    void LedgerMaster::abortLedgerClose()
    {
        mCurrentDB.endTransaction(false);
    }

    void LedgerMaster::loadLastKnownCLF()
    {
        bool needreset = true;
        uint256 lkcl = getLastClosedLedgerHash();
        if (lkcl.isNonZero()) {
            // there is a ledger in the database
            CanonicalLedgerForm::pointer newCLF = LegacyCLF::load(this, lkcl);
            if (newCLF) {
                mCurrentCLF = newCLF;
                mLastLedgerHash = lkcl;
                needreset = false;
            }
        }
        if (needreset) {
            reset();
        }
    }

    CanonicalLedgerForm::pointer LedgerMaster::catchUp(CanonicalLedgerForm::pointer updatedCurrentCLF)
    {
        // new SLE , old SLE
        SHAMap::Delta delta;
        bool needFull = false;

        WriteLog(ripple::lsINFO, ripple::Ledger) << "catching up from " << mCurrentCLF->getHash() << " to " << updatedCurrentCLF->getHash();

        if (mCurrentCLF->getHash().isZero())
        {
            needFull = true;
        }
        else
        {
            try {
                if (!updatedCurrentCLF->getDeltaSince(mCurrentCLF, delta))
                {
                    WriteLog(ripple::lsWARNING, ripple::Ledger) << "Could not compute delta";
                    needFull = true;
                }
            }
            catch (std::exception &e) {
                WriteLog(ripple::lsWARNING, ripple::Ledger) << "Could not compute delta " << e.what();
                needFull = true;
            }
        }

        if (needFull){
            return importLedgerState(updatedCurrentCLF->getHash());
        }

        // incremental update

        LedgerDatabase::ScopedTransaction tx(mCurrentDB);

        for(SHAMap::Delta::value_type &it: delta)
        {
            SLE::pointer newEntry = updatedCurrentCLF->getLegacyLedger()->getSLEi(it.first);
            SLE::pointer oldEntry = mCurrentCLF->getLegacyLedger()->getSLEi(it.first);

            if (newEntry)
            {
                LedgerEntry::pointer entry = LedgerEntry::makeEntry(newEntry);
                if (oldEntry)
                {	// SLE updated
                    if (entry) entry->storeChange();
                }
                else
                {	// SLE added
                    if (entry) entry->storeAdd();
                }
            }
            else
            { // SLE must have been deleted
                assert(oldEntry);
                LedgerEntry::pointer entry = LedgerEntry::makeEntry(oldEntry);
                if (entry) entry->storeDelete();
            }
        }
        updateDBFromLedger(updatedCurrentCLF);
        tx.endTransaction(true);

        setLastClosedLedger(updatedCurrentCLF);

        WriteLog(ripple::lsINFO, ripple::Ledger) << "done catching up with " << updatedCurrentCLF->getHash();

        return updatedCurrentCLF;
    }

    CanonicalLedgerForm::pointer LedgerMaster::importLedgerState(uint256 ledgerHash)
    {
        CanonicalLedgerForm::pointer res;

        WriteLog(ripple::lsINFO, ripple::Ledger) << "Importing full ledger " << ledgerHash;

        CanonicalLedgerForm::pointer newLedger = LegacyCLF::load(this, ledgerHash);

        if (newLedger) {
            // invalidates last closed ledger as we're about to destroy the database
            mCurrentDB.setState(LedgerDatabase::kLastClosedLedger, "");

            Database* db = mCurrentDB.getDBCon()->getDB();

            try {
                // delete all
                LedgerEntry::dropAll(mCurrentDB);

                int counter = 0, totalImports = 0;
                time_t start = time(nullptr);

                LedgerDatabase::ScopedTransaction tx(mCurrentDB);

                // import all anew
                newLedger->getLegacyLedger()->visitStateItems(
                    [&counter, &totalImports, &tx, this, start](SLE::ref curEntry) {
                        static const int kProgressCount = 100000;

                        totalImports++;

                        LedgerEntry::pointer entry = LedgerEntry::makeEntry(curEntry);
                        if (entry) {
                            entry->storeAdd();
                        }

                        if (++counter >= kProgressCount)
                        {
                            tx.endTransaction(true);
                            tx.beginTransaction(this->mCurrentDB);

                            int elapsed = time(nullptr) - start;
                            int rate = totalImports / (elapsed ? elapsed : 1);
                            WriteLog(ripple::lsINFO, ripple::Ledger) << "Imported " <<
                                totalImports << " items @" << rate;
                            counter = 0;

                            // reset the timer with every batch
                            Application& app(getApp());
                            LoadManager& mgr(app.getLoadManager());
                            mgr.resetDeadlockDetector();
                        }
                });

                WriteLog(ripple::lsINFO, ripple::Ledger) << "Imported " << totalImports << " items";

                updateDBFromLedger(newLedger);

                tx.endTransaction(true);
            }
            catch (...) {
                WriteLog(ripple::lsWARNING, ripple::Ledger) << "Could not import state";
                return CanonicalLedgerForm::pointer();
            }
            setLastClosedLedger(newLedger);
            res = newLedger;
        }
        return res;
    }

    void LedgerMaster::updateDBFromLedger(CanonicalLedgerForm::pointer ledger)
    {
        uint256 currentHash = ledger->getHash();
        string hex(to_string(currentHash));

        // saves last hash
        mCurrentDB.setState(LedgerDatabase::kLastClosedLedger, hex);

        ledger->save();
    }

    uint256 LedgerMaster::getLastClosedLedgerHash()
    {
        string h = mCurrentDB.getState(LedgerDatabase::kLastClosedLedger);
        return uint256(h); // empty string -> 0
    }

}
