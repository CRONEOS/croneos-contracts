#include <eosio/system.hpp>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

using namespace std;
using namespace eosio;

CONTRACT stakecron : public contract {
  public:
    using contract::contract;
    ACTION open(name staker, name ram_payer, name staketype);
    ACTION unstake(name staker, asset amount, name staketype);
    ACTION withdraw(name staker, uint64_t id);
    ACTION close(name staker, name staketype);

    ACTION setstaketype(name staketype, extended_asset stake_token, uint64_t release_sec,  bool enable);
    ACTION rmstaketype(name staketype);

    [[eosio::on_notify("*::transfer")]]
    void on_transfer(name from, name to, asset quantity, string memo);

  private:

    TABLE staketypes {
      name staketype;
      extended_asset stake_token;
      uint64_t release_sec = 60*60*24*3;
      bool enable;
      auto primary_key() const { return staketype.value; }
    };
    typedef eosio::multi_index<"staketypes"_n, staketypes> staketypes_table;

    //scoped by staketype
    TABLE stakes {
      name staker;
      asset amount;
      auto primary_key() const { return staker.value; }
    };
    typedef multi_index<name("stakes"), stakes> stakes_table;

    
    TABLE unstakes {
      uint64_t id;
      name staker;
      asset amount;
      name staketype;
      time_point_sec release_date;
      auto primary_key() const { return id; }
      uint64_t by_staker() const { return staker.value; }
    };
    typedef eosio::multi_index<"unstakes"_n, unstakes,
        eosio::indexed_by<"bystaker"_n, eosio::const_mem_fun<unstakes, uint64_t, &unstakes::by_staker>>
    > unstakes_table;

    staketypes get_stake_type(const name& staketype);
};
