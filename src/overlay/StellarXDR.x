// things we need to serialize
//   protocol messages
//   FBA messages for signing
//   tx for signing
//   tx sets for FBA
//   tx sets for history
//   Bucket list for hashing
//   array of ledgerentries for sending deltas

namespace stellarxdr {


// messages
typedef opaque uint256[32];
typedef opaque uint160[20];
typedef opaque decimal128[16];
typedef unsigned hyper uint64;
typedef unsigned uint32;

struct Error
{
    int code;
    string msg<100>;
};

struct Hello
{
	int protocolVersion;
	string versionStr<100>;
	int port;
};

enum TransactionType
{
	PAYMENT,
	CREATE_OFFER,
	CANCEL_OFFER,
	CHANGE_ACCOUNT,
	CHANGE_TRUST,
	ACCOUNT_MERGE,
	INFLATION
};

struct CurrencyIssuer
{
	opaque currency<20>;
	uint160 issuer;
};

struct KeyValue
{
	uint32 key;
	opaque value<64>;
};

struct PaymentTx
{
	uint160 destination;  
	opaque currency<20>;	// what they end up with
	uint64 amount;			// amount they end up with
	CurrencyIssuer path<>;	// what hops it must go through to get there
	uint64 sendMax;			// the maximum amount of the source currency this
							// will send. The tx will fail if can't be met
	opaque memo<32>;
	opaque sourceMemo<32>;	// used to return a payment
};

struct CreateOfferTx
{
	CurrencyIssuer currencyTakerGets;
	uint64 amountTakerGets;
	CurrencyIssuer currencyTakerPays;
	uint64 amountTakerPays;

	uint32 offerSeqNum;		// set if you want to change an existing offer
	bool passive;	// only take offers that cross this. not offers that match it
};

struct ChangeAccountTx
{
	uint256 setAuthKey;
	uint256 signingKey;
	KeyValue data;
	uint32	flags;
	uint32 transferRate;
};

struct ChangeTrustTx
{
	CurrencyIssuer line;
	uint64 amount;
	bool auth;
};

struct Transaction
{
    uint160 account;
	uint32 maxFee;
	uint32 seqNum;
	uint32 maxLedger;	// maximum ledger this tx is valid to be applied in

	union switch (TransactionType type)
	{
		case PAYMENT:
			PaymentTx paymentTx;
		case CREATE_OFFER:
			CreateOfferTx createOfferTx;
		case CANCEL_OFFER:
			uint32 offerSeqNum;
		case CHANGE_ACCOUNT:
			ChangeAccountTx changeAccountTx;
		case CHANGE_TRUST:
			ChangeTrustTx changeTrustTx;
		case ACCOUNT_MERGE:
			uint160 destination;
		case INFLATION:
			uint32 inflationSeq;
	} body;
};

struct TransactionEnvelope
{
    Transaction tx;
    uint256 signature;
};

struct TransactionSet
{
    TransactionEnvelope txs<>;
};

struct LedgerHeader
{
	uint256 hash;
    uint256 previousLedgerHash;
    uint256 txSetHash;
	uint256 clfHash;

	
	uint64 totalCoins;
	uint64 feePool;
	uint32 ledgerSeq;
	uint32 inflationSeq;
	uint64 baseFee;
	uint64 closeTime;       
};


struct LedgerHistory
{
    LedgerHeader header;
    TransactionSet txSet;
};

struct History
{
    int fromLedger;
    int toLedger;
    LedgerHistory ledgers<>;
};



// FBA  messages
struct Ballot
{
	int index;						// n
    uint256 txSetHash;				// x
	uint64 closeTime;				// x
	uint32 baseFee;					// x
};

struct SlotBallot
{
	uint32 ledgerIndex;				// the slot				
	uint256 previousLedgerHash;		// the slot

    Ballot ballot;
};

enum FBAStatementType
{
	PREPARE,
	PREPARED,
	COMMIT,
	COMMITTED,
	EXTERNALIZED,
	UNKNOWN
};

struct FBAContents
{
	SlotBallot slotBallot;
	uint256 quorumSetHash;
	
	union switch (FBAStatementType type)
	{
		case PREPARE:
			Ballot excepted<>;
		case PREPARED:
		case COMMIT:
		case COMMITTED:
		case EXTERNALIZED:
		case UNKNOWN:
			void;		
	} body;
};

struct FBAEnvelope
{
	uint256 nodeID;
    uint256 signature;
	FBAContents contents;
};

enum LedgerTypes {
  ACCOUNT,
  TRUSTLINE,
  OFFER
};

struct Amount
{
    uint64 value;
    uint160 currency;
    uint160 issuer;
};



struct AccountEntry
{
    uint160 accountID;
    uint64 balance;
    uint32 sequence;
    uint32 ownerCount;
    uint32 transferRate;
    uint256 pubKey;
	uint160 inflationDest;
	uint256 creditAuthKey;
	KeyValue data<>;

	uint32 flags; // require dt, require auth, 
};



struct TrustLineEntry
{
    uint160 lowAccount;
    uint160 highAccount;
    uint160 currency;
    uint64 lowLimit;
    uint64 highLimit;
    uint64 balance;
    bool lowAuthSet;  // if the high account has authorized the low account to hold its credit
    bool highAuthSet;
};

struct OfferEntry
{
    uint160 accountID;
    uint32 sequence;
	CurrencyIssuer takerGets;
	CurrencyIssuer takerPays;
	uint64 takerGetsAmount;
	uint64 takerPaysAmount;

    bool passive;
};

union LedgerEntry switch (LedgerTypes type)
{
 case ACCOUNT:
   AccountEntry account;
 case TRUSTLINE:
   TrustLineEntry trustLine;
 case OFFER:
      OfferEntry offer;
};

struct QuorumSet
{
    uint32 threshold;
    uint256 validators<>;
};

struct Peer
{
    uint32 ip;
    uint32 port;
};

enum MessageType
{
	ERROR_MSG,	
	HELLO,
	DONT_HAVE,
	
	GET_PEERS,   // gets a list of peers this guy knows about		
	PEERS,		
		
	GET_HISTORY,  // gets a list of tx sets in the given range		
	HISTORY,		

	GET_DELTA,  // gets all the bucket list changes since a particular ledger index		
	DELTA,		
		
	GET_TX_SET,  // gets a particular txset by hash		
	TX_SET,	
		
	GET_VALIDATIONS, // gets validations for a given ledger hash		
	VALIDATIONS,	
		
	TRANSACTION, //pass on a tx you have heard about		
	JSON_TRANSACTION,
		
	// FBA		
	GET_QUORUMSET,		
	QUORUMSET,	
		
	FBA_MESSAGE
};

struct DontHave
{
	MessageType type;
	uint256 reqHash;
};

struct GetDelta
{
    uint256 oldLedgerHash;
    uint32 oldLedgerSeq;
};

struct Delta
{
	LedgerHeader ledgerHeader;
	LedgerEntry deltaEntries<>;
};

struct GetHistory
{
    uint256 a;
    uint256 b;
};


union StellarMessage switch (MessageType type) {
	case ERROR_MSG:
		Error error;
	case HELLO:
		Hello hello;
	case DONT_HAVE:
		DontHave dontHave;
	case GET_PEERS:
		void;
	case PEERS:
		Peer peers<>;
	case GET_HISTORY:
		GetHistory historyReq;
	case HISTORY:
		History history;
	case GET_DELTA:	
		GetDelta deltaReq;	
	case DELTA:	
		Delta delta;

	case GET_TX_SET:
		uint256 txSetHash;		
	case TX_SET:
		TransactionSet txSet;

	case GET_VALIDATIONS:	
		uint256 ledgerHash;	
	case VALIDATIONS:
		FBAEnvelope validations<>;

	case TRANSACTION:
		TransactionEnvelope transaction;

	// FBA		
	case GET_QUORUMSET:		
		uint256 qSetHash;	
	case QUORUMSET:
		QuorumSet quorumSet;

	case FBA_MESSAGE:
		FBAEnvelope fbaMessage;
};


}
