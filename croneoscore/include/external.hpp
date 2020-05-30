struct currency_stats {
    asset    supply;
    asset    max_supply;
    name     issuer;

    uint64_t primary_key()const { return supply.symbol.code().raw(); }
};
typedef eosio::multi_index< "stat"_n, currency_stats > tokenstats_table;