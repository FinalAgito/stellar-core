// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the ISC License. See the COPYING file at the top-level directory of
// this distribution or at http://opensource.org/licenses/ISC

#include "ledger/TrustFrame.h"
#include "ledger/AccountFrame.h"
#include "crypto/Base58.h"
#include "crypto/SHA.h"
#include "lib/json/json.h"
#include "database/Database.h"
#include "LedgerDelta.h"
#include "util/types.h"

using namespace std;
using namespace soci;

namespace stellar {
    const char *TrustFrame::kSQLCreateStatement =
        "CREATE TABLE IF NOT EXISTS TrustLines            \
         (                                                \
         accountID     VARCHAR(51)  NOT NULL,             \
         issuer        VARCHAR(51)  NOT NULL,             \
         isoCurrency   VARCHAR(4)   NOT NULL,             \
         tlimit        BIGINT       NOT NULL DEFAULT 0    \
                                    CHECK (tlimit >= 0),  \
         balance       BIGINT       NOT NULL DEFAULT 0    \
                                    CHECK (balance >= 0), \
         authorized    BOOL         NOT NULL,             \
         PRIMARY KEY (accountID, issuer, isoCurrency)     \
         );";

    TrustFrame::TrustFrame() : EntryFrame(TRUSTLINE), mTrustLine(mEntry.trustLine())
    {
    }

    TrustFrame::TrustFrame(LedgerEntry const& from) : EntryFrame(from), mTrustLine(mEntry.trustLine())
    {
    }

    TrustFrame::TrustFrame(TrustFrame const &from) : TrustFrame(from.mEntry)
    {
    }

    TrustFrame& TrustFrame::operator=(TrustFrame const& other)
    {
        if (&other != this)
        {
            mTrustLine = other.mTrustLine;
            mKey = other.mKey;
            mKeyCalculated = other.mKeyCalculated;
        }
        return *this;
    }

    void TrustFrame::getKeyFields(LedgerKey const& key,
                                  std::string& base58AccountID,
                                  std::string& base58Issuer,
                                  std::string& currencyCode)
    {
        base58AccountID = toBase58Check(VER_ACCOUNT_ID, key.trustLine().accountID);
        base58Issuer = toBase58Check(VER_ACCOUNT_ID, key.trustLine().currency.isoCI().issuer);
        currencyCodeToStr(key.trustLine().currency.isoCI().currencyCode, currencyCode);
    }

    int64_t TrustFrame::getBalance()
    {
        assert(isValid());
        return mTrustLine.balance;
    }

    bool TrustFrame::isValid() const
    {
        const TrustLineEntry &tl = mTrustLine;
        bool res = tl.currency.type() != NATIVE;
        res = res && (tl.balance >= 0);
        res = res && (tl.balance <= tl.limit);
        return res;
    }

    bool TrustFrame::exists(Database& db, LedgerKey const& key)
    {
        std::string b58AccountID, b58Issuer, currencyCode;
        getKeyFields(key, b58AccountID, b58Issuer, currencyCode);
        int exists = 0;
        db.getSession() <<
            "SELECT EXISTS (SELECT NULL FROM TrustLines \
             WHERE accountID=:v1 and issuer=:v2 and isoCurrency=:v3)",
            use(b58AccountID), use(b58Issuer), use(currencyCode),
            into(exists);
        return exists != 0;
    }


    void TrustFrame::storeDelete(LedgerDelta &delta, Database& db)
    {
        storeDelete(delta, db, getKey());
    }

    void TrustFrame::storeDelete(LedgerDelta &delta, Database& db, LedgerKey const& key)
    {
        std::string b58AccountID, b58Issuer, currencyCode;
        getKeyFields(key, b58AccountID, b58Issuer, currencyCode);

        db.getSession() <<
            "DELETE from TrustLines \
             WHERE accountID=:v1 and issuer=:v2 and isoCurrency=:v3",
            use(b58AccountID), use(b58Issuer), use(currencyCode);

        delta.deleteEntry(key);
    }

    void TrustFrame::storeChange(LedgerDelta &delta, Database& db)
    {
        assert(isValid());

        std::string b58AccountID, b58Issuer, currencyCode;
        getKeyFields(getKey(), b58AccountID, b58Issuer, currencyCode);

        statement st =
            (db.getSession().prepare <<
             "UPDATE TrustLines \
              SET balance=:b, tlimit=:tl, authorized=:a \
              WHERE accountID=:v1 and issuer=:v2 and isoCurrency=:v3",
             use(mTrustLine.balance),
             use(mTrustLine.limit),
             use((int)mTrustLine.authorized),
             use(b58AccountID), use(b58Issuer), use(currencyCode));

        st.execute(true);

        if (st.get_affected_rows() != 1)
        {
            throw std::runtime_error("Could not update data in SQL");
        }

        delta.modEntry(*this);
    }

    void TrustFrame::storeAdd(LedgerDelta &delta, Database& db)
    {
        assert(isValid());

        std::string b58AccountID, b58Issuer, currencyCode;
        getKeyFields(getKey(), b58AccountID, b58Issuer, currencyCode);

        statement st =
            (db.getSession().prepare <<
                "INSERT INTO TrustLines (accountID, issuer, isoCurrency, tlimit, authorized) \
                 VALUES (:v1,:v2,:v3,:v4,:v5)",
             use(b58AccountID), use(b58Issuer), use(currencyCode),
             use(mTrustLine.limit),
             use((int)mTrustLine.authorized));

        st.execute(true);

        if (st.get_affected_rows() != 1)
        {
            throw std::runtime_error("Could not update data in SQL");
        }

        delta.addEntry(*this);
    }

    static const char *trustLineColumnSelector = "SELECT accountID, issuer, isoCurrency, tlimit,balance,authorized FROM TrustLines";

    bool TrustFrame::loadTrustLine(const uint256& accountID,
        const Currency& currency,
        TrustFrame& retLine, Database& db)
    {
        std::string accStr, issuerStr, currencyStr;

        accStr = toBase58Check(VER_ACCOUNT_ID, accountID);
        currencyCodeToStr(currency.isoCI().currencyCode, currencyStr);
        issuerStr = toBase58Check(VER_ACCOUNT_ID, currency.isoCI().issuer);

        session &session = db.getSession();

        details::prepare_temp_type sql = (session.prepare <<
            trustLineColumnSelector << " WHERE accountID=:id AND "\
            "issuer=:issuer AND isoCurrency=:currency",
            use(accStr), use(issuerStr), use(currencyStr));

        bool res = false;

        loadLines(sql, [&retLine, &res](TrustFrame const &trust)
        {
            retLine = trust;
            res = true;
        });
        return res;
    }

    void TrustFrame::loadLines(details::prepare_temp_type &prep,
        std::function<void(const TrustFrame&)> trustProcessor)
    {
        string accountID;
        std::string issuer, currency;
        int authorized;

        TrustFrame curTrustLine;

        TrustLineEntry &tl = curTrustLine.mTrustLine;

        statement st = (prep,
            into(accountID), into(issuer), into(currency), into(tl.limit),
            into(tl.balance), into(authorized)
            );

        st.execute(true);
        while (st.got_data())
        {
            tl.accountID = fromBase58Check256(VER_ACCOUNT_ID, accountID);
            tl.currency.type(ISO4217);
            tl.currency.isoCI().issuer = fromBase58Check256(VER_ACCOUNT_ID, issuer);
            strToCurrencyCode(tl.currency.isoCI().currencyCode, currency);
            tl.authorized = authorized;
            trustProcessor(curTrustLine);

            st.fetch();
        }
    }

    void TrustFrame::loadLines(const uint256& accountID,
        std::vector<TrustFrame>& retLines, Database& db)
    {
        std::string accStr;
        accStr = toBase58Check(VER_ACCOUNT_ID, accountID);

        session &session = db.getSession();

        details::prepare_temp_type sql = (session.prepare <<
            trustLineColumnSelector << " WHERE accountID=:id",
            use(accStr));

        loadLines(sql, [&retLines](TrustFrame const &cur)
        {
            retLines.push_back(cur);
        });
    }


    void TrustFrame::dropAll(Database &db)
    {
        db.getSession() << "DROP TABLE IF EXISTS TrustLines;";
        db.getSession() << kSQLCreateStatement;
    }
}
