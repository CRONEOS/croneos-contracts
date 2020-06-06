#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>

using namespace std;
using namespace eosio;

CONTRACT stakecron : public contract {
  public:
    using contract::contract;
    ACTION open(name staker, name ram_payer);
    ACTION unstake(name staker, asset amount);
    ACTION withdraw(name staker, uint64_t id);
    ACTION close(name staker);

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(name from, name to, asset quantity, string memo);

    TABLE settings {
      extended_asset stake_token;
      uint64_t release_sec = 60*60*24*3;
    };
    typedef eosio::singleton<"settings"_n, settings> settings_table;

    ACTION setsettings(settings new_settings);

  private:


    TABLE stakes {
      name    staker;
      asset  amount;
      auto primary_key() const { return staker.value; }
    };
    typedef multi_index<name("stakes"), stakes> stakes_table;

    TABLE unstakes {
      uint64_t id;
      name    staker;
      asset  amount;
      time_point_sec release_date;
      auto primary_key() const { return id; }
      uint64_t by_staker() const { return staker.value; }
    };
    typedef eosio::multi_index<"unstakes"_n, unstakes,
        eosio::indexed_by<"bystaker"_n, eosio::const_mem_fun<unstakes, uint64_t, &unstakes::by_staker>>
    > unstakes_table;
};
