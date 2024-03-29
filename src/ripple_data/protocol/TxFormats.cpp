//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

namespace ripple {

TxFormats::TxFormats ()
{
    add ("AccountSet", ttACCOUNT_SET)
            << SOElement (sfTransferRate,    SOE_OPTIONAL)
            << SOElement (sfSetFlag,         SOE_OPTIONAL)
            << SOElement (sfClearFlag,       SOE_OPTIONAL)
            << SOElement (sfInflationDest,   SOE_OPTIONAL)
            ;

    add("AccountMerge", ttACCOUNT_MERGE)
            << SOElement (sfDestination,     SOE_REQUIRED)
			<< SOElement (sfDestinationTag,  SOE_OPTIONAL)
            ;

    add ("TrustSet", ttTRUST_SET)
            << SOElement (sfLimitAmount,     SOE_OPTIONAL)
            << SOElement (sfQualityIn,       SOE_OPTIONAL)
            << SOElement (sfQualityOut,      SOE_OPTIONAL)
            ;

    add ("OfferCreate", ttOFFER_CREATE)
            << SOElement (sfTakerPays,       SOE_REQUIRED)
            << SOElement (sfTakerGets,       SOE_REQUIRED)
            << SOElement (sfExpiration,      SOE_OPTIONAL)
            << SOElement (sfOfferSequence,   SOE_OPTIONAL)
            ;

    add ("OfferCancel", ttOFFER_CANCEL)
            << SOElement (sfOfferSequence,   SOE_REQUIRED)
            ;

    add ("SetRegularKey", ttREGULAR_KEY_SET)
            << SOElement (sfRegularKey,  SOE_OPTIONAL)
            ;

    add ("Payment", ttPAYMENT)
            << SOElement (sfDestination,     SOE_REQUIRED)
            << SOElement (sfAmount,          SOE_REQUIRED)
            << SOElement (sfSendMax,         SOE_OPTIONAL)
            << SOElement (sfPaths,           SOE_DEFAULT)
            << SOElement (sfInvoiceID,       SOE_OPTIONAL)
            << SOElement (sfDestinationTag,  SOE_OPTIONAL)
            ;

	add("Inflation", ttINFLATION)
		<< SOElement(sfInflateSeq, SOE_REQUIRED);
/*	
    add ("Contract", ttCONTRACT)
            << SOElement (sfExpiration,      SOE_REQUIRED)
            << SOElement (sfBondAmount,      SOE_REQUIRED)
            << SOElement (sfStampEscrow,     SOE_REQUIRED)
            << SOElement (sfRippleEscrow,    SOE_REQUIRED)
            << SOElement (sfCreateCode,      SOE_OPTIONAL)
            << SOElement (sfFundCode,        SOE_OPTIONAL)
            << SOElement (sfRemoveCode,      SOE_OPTIONAL)
            << SOElement (sfExpireCode,      SOE_OPTIONAL)
            ;

    add ("RemoveContract", ttCONTRACT_REMOVE)
            << SOElement (sfTarget,          SOE_REQUIRED)
            ;
			*/
    add ("EnableAmendment", ttAMENDMENT)
            << SOElement (sfAmendment,         SOE_REQUIRED)
            ;

    add ("SetFee", ttFEE)
            << SOElement (sfBaseFee,             SOE_REQUIRED)
            << SOElement (sfReferenceFeeUnits,   SOE_REQUIRED)
            << SOElement (sfReserveBase,         SOE_REQUIRED)
            << SOElement (sfReserveIncrement,    SOE_REQUIRED)
            ;
}

void TxFormats::addCommonFields (Item& item)
{
    item
        << SOElement(sfTransactionType,     SOE_REQUIRED)
        << SOElement(sfFlags,               SOE_OPTIONAL)
        << SOElement(sfSourceTag,           SOE_OPTIONAL)
        << SOElement(sfAccount,             SOE_REQUIRED) 
        << SOElement(sfSequence,            SOE_REQUIRED) 
        << SOElement(sfPreviousTxnID,       SOE_OPTIONAL) // Deprecated: Do not use
        << SOElement(sfLastLedgerSequence,  SOE_OPTIONAL)
        << SOElement(sfAccountTxnID,        SOE_OPTIONAL)
        << SOElement(sfFee,                 SOE_REQUIRED) 
        << SOElement(sfOperationLimit,      SOE_OPTIONAL) 
        << SOElement(sfMemos,               SOE_OPTIONAL)
        << SOElement(sfSigningPubKey,       SOE_REQUIRED) 
        << SOElement(sfTxnSignature,        SOE_OPTIONAL)
        ;
}

TxFormats* TxFormats::getInstance ()
{
    static TxFormats instance;
    //return SharedSingleton <TxFormats>::getInstance ();
    return &instance;
}

} // ripple
