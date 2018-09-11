#include "fairdicegame.hpp"

void fairdicegame::reveal(uint64_t id, checksum256 seed) {
    require_auth(REVEALER);
    st_bet bet = find_or_error(id);
    assert_seed(seed, bet.seed_hash);

    uint8_t random_roll = compute_random_roll(seed);
    asset payout = asset(0, EOS_SYMBOL);
    if (random_roll < bet.roll_under) {
        payout = compute_payout(bet.roll_under, bet.amount);
        action(permission_level{_self, N(active)},
               N(eosio.token),
               N(transfer),
               make_tuple(_self, bet.player, payout, string("")))
            .send();
    }
    unlock(bet.amount);
    if (bet.referrer != _self) {
        action(
            permission_level{_self, N(active)},
            N(eosio.token),
            N(transfer),
            make_tuple(
                _self, bet.referrer, comput_referrer_reward(bet), string("")))
            .send();
    }
    remove(bet);
    st_result result{.bet_id = bet.id,
                     .player = bet.player,
                     .referrer = bet.referrer,
                     .amount = bet.amount,
                     .roll_under = bet.roll_under,
                     .random_roll = random_roll,
                     .seed = seed,
                     .seed_hash = bet.seed_hash,
                     .payout = payout};
    action(permission_level{_self, N(active)},
           LOG,
           N(result),
           result)
        .send();
}

void fairdicegame::offer(account_name from,
                         account_name to,
                         asset quantity,
                         string memo) {
    if (from == _self || to != _self) {
        return;
    }
    eosio_assert(quantity.symbol == EOS_SYMBOL, "quantity must be EOS symbol");
    eosio_assert(quantity.is_valid(), "quantity invalid");
    eosio_assert(quantity.amount >= 1000, "quantity must be greater than 0.1");

    // remove space
    memo.erase(std::remove_if(memo.begin(),
                              memo.end(),
                              [](unsigned char x) { return std::isspace(x); }),
               memo.end());

    string roll_under_str;
    string seed_hash_str;
    string expiration_str;
    string signature_str;
    string referrer_str;

    uint8_t roll_under;
    checksum256 seed_hash;
    uint64_t expiration;
    account_name referrer = _self;

    size_t pos;
    pos = sub2sepa(memo, &roll_under_str, '-', 0, true);
    pos = sub2sepa(memo, &seed_hash_str, '-', ++pos, true);
    pos = sub2sepa(memo, &expiration_str, '-', ++pos, true);

    if (memo.find('-', pos + 1) != string::npos) {
        pos = sub2sepa(memo, &referrer_str, '-', ++pos, true);
    }
    signature_str = memo.substr(++pos);

    eosio_assert(!signature_str.empty(), "no signature");

    roll_under = static_cast<uint8_t>(stoi(roll_under_str));

    eosio_assert(roll_under >= 2 && roll_under <= 96,
                 "roll_under overflow, must be greater than 2 and less than 96");

    eosio_assert(
        max_payout(roll_under, quantity) <= max_bonus(),
        "offered overflow, expected earning is greater than the maximum bonus");

    eosio_assert(!seed_hash_str.empty(), "no seed");
    seed_hash = hex_to_sha256(seed_hash_str);
    assert_hash(seed_hash);

    expiration = stoull(expiration_str);
    eosio_assert(expiration > now(), "seed hash expired");

    string signed_from = memo.substr(0, memo.find_last_of('-'));

    assert_signature(signed_from, signature_str);

    if (!referrer_str.empty()) {
        referrer = string_to_name(referrer_str.c_str());
    }

    eosio_assert(referrer != from, "referrer can not be self");

    st_bet _bet{.id = _bets.available_primary_key(),
                .player = from,
                .referrer = referrer,
                .amount = quantity,
                .roll_under = roll_under,
                .seed_hash = seed_hash,
                .created_at = now()};
    save(_bet, expiration);
    lock(quantity);
    action(permission_level{_self, N(active)},
           _self,
           N(receipt),
           _bet)
        .send();
}

void fairdicegame::receipt(st_bet bet) {
    require_auth(_self);
}
