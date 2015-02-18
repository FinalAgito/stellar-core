#include "ledger/LedgerDelta.h"

namespace stellar
{
    void LedgerDelta::addEntry(EntryFrame const& entry)
    {
        addEntry(entry.copy());
    }

    void LedgerDelta::deleteEntry(EntryFrame const& entry)
    {
        deleteEntry(entry.copy());
    }

    void LedgerDelta::modEntry(EntryFrame const& entry)
    {
        modEntry(entry.copy());
    }

    void LedgerDelta::addEntry(EntryFrame::pointer entry)
    {
        auto k = entry->getKey();
        auto del_it = mDelete.find(k);
        if (del_it != mDelete.end())
        {
            // delete + new is an update
            mDelete.erase(del_it);
            mMod[k] = entry;
        }
        else
        {
            assert(mNew.find(k) == mNew.end()); // double new
            assert(mMod.find(k) == mMod.end()); // mod + new is invalid
            mNew[k] = entry;
        }
    }

    void LedgerDelta::deleteEntry(EntryFrame::pointer entry)
    {
        auto k = entry->getKey();
        deleteEntry(k);
    }

    void LedgerDelta::deleteEntry(LedgerKey const& k)
    {
        auto new_it = mNew.find(k);
        if (new_it != mNew.end())
        {
            // new + delete -> don't add it in the first place
            mNew.erase(new_it);
        }
        else
        {
            assert(mDelete.find(k) == mDelete.end()); // double delete is invalid
            // only keep the delete
            mMod.erase(k);
            mDelete.insert(k);
        }
    }

    void LedgerDelta::modEntry(EntryFrame::pointer entry)
    {
        auto k = entry->getKey();
        auto mod_it = mMod.find(k);
        if ( mod_it != mMod.end())
        {
            // collapse mod
            mod_it->second = entry;
        }
        else
        {
            auto new_it = mNew.find(k);
            if (new_it != mNew.end())
            {
                // new + mod = new (with latest value)
                new_it->second = entry;
            }
            else
            {
                assert(mDelete.find(k) == mDelete.end()); // delete + mod is illegal
                mMod[k] = entry;
            }
        }
    }

    void LedgerDelta::merge(LedgerDelta &other)
    {
        for (auto &d : other.mDelete)
        {
            deleteEntry(d);
        }
        for (auto &n : other.mNew)
        {
            addEntry(n.second);
        }
        for (auto &m : other.mMod)
        {
            modEntry(m.second);
        }
    }

    xdr::msg_ptr LedgerDelta::getTransactionMeta() const
    {
        TransactionMeta tm;
        
        CLFEntryBase::_entry_t et;

        et.type(LIVEENTRY);

        for (auto const &k : mNew)
        {
            et.liveEntry() = k.second->mEntry;
            tm.entries.push_back(CLFEntryBase(et));
        }
        for (auto const &k : mMod)
        {
            et.liveEntry() = k.second->mEntry;
            tm.entries.push_back(CLFEntryBase(et));
        }

        et.type(DEADENTRY);

        for (auto const &k : mDelete)
        {
            et.deadEntry() = k;
            tm.entries.push_back(CLFEntryBase(et));
        }

        return xdr::xdr_to_msg(tm);
    }
}
