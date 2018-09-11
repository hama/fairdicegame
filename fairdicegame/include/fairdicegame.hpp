#include "eosio.token.hpp"
#include "types.hpp"

class fairdicegame : public contract {
   public:
    fairdicegame(account_name self)
        : contract(self),
          _bets(_self, _self),
          _fund_pool(_self, _self),
          _hash(_self, _self),
          _global(_self, _self){};

    void offer(account_name from, account_name to, asset quantity, string memo);

    // @abi action
    void receipt(st_bet bet);

    // @abi action
    void reveal(uint64_t id, checksum256 seed);

   private:
    tb_bets _bets;
    tb_fund_pool _fund_pool;
    tb_hash _hash;
    tb_global _global;

    uint8_t compute_random_roll(const checksum256& seed) {
        return uint64_hash(seed) % 100 + 1;
    }

    asset comput_referrer_reward(const st_bet& bet) { return bet.amount / 200; }

    uint64_t next_id() {
        st_global global = _global.get_or_default(
            st_global{.current_id = _bets.available_primary_key()});
        global.current_id += 1;
        _global.set(global, _self);
        return global.current_id;
    }

    string referrer_memo(const st_bet& bet) {
        string memo = "bet id:";
        string id = uint64_string(bet.id);
        memo.append(id);
        memo.append(" player: ");
        string player = name{bet.player}.to_string();
        memo.append(player);
        memo.append(" referral reward! - dapp.pub/dice/");
        return memo;
    }

    string winner_memo(const st_bet& bet) {
        string memo = "bet id:";
        string id = uint64_string(bet.id);
        memo.append(id);
        memo.append(" player: ");
        string player = name{bet.player}.to_string();
        memo.append(player);
        memo.append(" winner! - dapp.pub/dice/");
        print(memo);
        return memo;
    }

    st_bet find_or_error(const uint64_t& id) {
        auto itr = _bets.find(id);
        eosio_assert(itr != _bets.end(), "bet not found");
        return *itr;
    }

    void assert_hash(const checksum256& seed_hash) {
        const uint64_t key = uint64_hash(seed_hash);
        auto itr = _hash.find(key);
        eosio_assert(itr == _hash.end(), "hash duplicate");
    }

    void cleanup() {
        auto index = _hash.get_index<N(by_expiration)>();
        auto upper_itr = index.upper_bound(now());
        auto itr = index.begin();
        while (itr != upper_itr) {
            itr = index.erase(itr);
        }
    }

    void save(const st_bet& bet, const uint64_t& expiration) {
        _bets.emplace(_self, [&](st_bet& r) {
            r.id = bet.id;
            r.player = bet.player;
            r.referrer = bet.referrer;
            r.amount = bet.amount;
            r.roll_under = bet.roll_under;
            r.seed_hash = bet.seed_hash;
            r.created_at = bet.created_at;
        });
        cleanup();
        _hash.emplace(_self, [&](st_hash& r) {
            r.hash = bet.seed_hash;
            r.expiration = expiration;
        });
    }

    void remove(const st_bet& bet) { _bets.erase(bet); }

    void unlock(const asset& amount) {
        st_fund_pool pool = get_fund_pool();
        pool.locked -= amount;
        eosio_assert(pool.locked.amount >= 0, "fund unlock error");
        _fund_pool.set(pool, _self);
    }

    void lock(const asset& amount) {
        st_fund_pool pool = get_fund_pool();
        pool.locked += amount;
        _fund_pool.set(pool, _self);
    }

    asset compute_payout(const uint8_t& roll_under, const asset& offer) {
        return min(max_payout(roll_under, offer), max_bonus());
    }
    asset max_payout(const uint8_t& roll_under, const asset& offer) {
        const double ODDS = 98.0 / ((double)roll_under - 1.0);
        return asset(ODDS * offer.amount, offer.symbol);
    }

    asset max_bonus() { return available_balance() / 100; }

    asset available_balance() {
        auto token = eosio::token(N(eosio.token));
        const asset balance =
            token.get_balance(_self, symbol_type(EOS_SYMBOL).name());
        const asset locked = get_fund_pool().locked;
        const asset available = balance - locked;
        eosio_assert(available.amount >= 0, "fund pool overdraw");
        return available;
    }

    st_fund_pool get_fund_pool() {
        st_fund_pool fund_pool{.locked = asset(0, EOS_SYMBOL)};
        return _fund_pool.get_or_create(_self, fund_pool);
    }

    void assert_signature(const string& data, const string& sig) {
        checksum256 digest;
        const char* data_cstr = data.c_str();
        sha256(data_cstr, strlen(data_cstr), &digest);
        signature _sig = str_to_sig(sig);
        public_key _key = str_to_pub(PUB_KEY);
        assert_recover_key(&digest,
                           (char*)&_sig.data,
                           sizeof(_sig.data),
                           (char*)&_key.data,
                           sizeof(_key.data));
    }

    void assert_seed(const checksum256& seed, const checksum256& hash) {
        string seed_str = sha256_to_hex(seed);
        assert_sha256(seed_str.c_str(),
                      strlen(seed_str.c_str()),
                      (const checksum256*)&hash);
    }
};

extern "C" {
void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    fairdicegame thiscontract(receiver);

    if ((code == N(eosio.token)) && (action == N(transfer))) {
        execute_action(&thiscontract, &fairdicegame::offer);
        return;
    }

    if (code != receiver) return;

    switch (action) { EOSIO_API(fairdicegame, (receipt)(reveal)) };
    eosio_exit(0);
}
}
