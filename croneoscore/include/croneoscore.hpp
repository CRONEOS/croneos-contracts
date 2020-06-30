#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/singleton.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/action.hpp>
#include <eosio/time.hpp>
#include <eosio/permission.hpp>

//comment out to enable dev actions
#define _DEV_ 

using namespace std;
using namespace eosio;

CONTRACT croneoscore : public contract {
  public:
    using contract::contract;

    struct oracle_src{
      string api_url;
      string json_path;
    };

    struct scope_meta{
      string about;
      string logo;
    };

    ACTION qschedule(
        name owner,
        uint64_t queue_id,
        name tick_action_name, //tick
        uint32_t delay_sec, 
        uint32_t expiration_sec, 
        asset gas_fee,
        string description,
        uint8_t repeat
    );

    ACTION schedule(
        name owner,
        name scope, 
        name tag,
        name auth_bouncer,
        vector<action> actions, 
        time_point_sec due_date, 
        uint32_t delay_sec, 
        time_point_sec expiration, 
        uint32_t expiration_sec, 
        asset gas_fee, 
        string description,
        vector<oracle_src> oracle_srcs
    );
    ACTION cancelbytag(name owner, name tag, uint8_t size, name scope);
    ACTION cancel(name owner, uint64_t id, name scope);
    ACTION exec(name executer, uint64_t id, name scope, std::vector<char> oracle_response);

    ACTION setsettings(uint8_t max_allowed_actions, vector<permission_level> required_exec_permission, uint8_t reward_fee_perc, asset new_scope_fee, name token_contract);

    ACTION resetstate();

    ACTION withdraw( name miner, asset amount );
    ACTION refund(name owner, asset amount);
    ACTION movefund(name receiver, asset amount);

    //approve or unapprove a trusted dapp contract
    ACTION approvedapp(name contract, bool approved);
    ACTION regdapp(name contract, string description, string url, string logo);
    ACTION unregdapp(name contract);

    ACTION setexecacc (name owner, permission_level exec_permission, bool remove);
    ACTION setprivscope (name actor, name scope_owner, name scope, bool remove);
    ACTION setscopemeta (name owner, name scope, scope_meta meta);
    ACTION setscopeuser (name owner, name scope, name user, bool remove);
    
    ACTION addgastoken(extended_asset gas_token);
    ACTION rmgastoken(asset gas_token);

    ACTION setgasvalue(symbol_code symbol, symbol_code smart_symbol, double init_value, bool remove);

#ifdef _DEV_
    ACTION delrewards(name scope);
    ACTION delsettings();
    ACTION clear();
#endif
    
    //notification handlers
    [[eosio::on_notify("*::transfer")]]
    void top_up_balance(name from, name to, asset quantity, string memo);


  private:
  //************************
    TABLE cronjobs {
      uint64_t id;
      name owner;
      name tag;
      name auth_bouncer;
      vector<action> actions;
      time_point_sec submitted;
      time_point_sec due_date;//calculated now + delay_sec
      time_point_sec expiration;
      asset gas_fee;
      string description;
      uint8_t max_exec_count=1;
      vector<oracle_src> oracle_srcs;
      
      //uint8_t job_type = 0; //0:normal, 1: repeating
      
      uint64_t primary_key() const { return id; }
      uint64_t by_owner() const { return owner.value; }
      uint64_t by_due_date() const { return due_date.sec_since_epoch(); }
      uint128_t by_owner_tag() const { return (uint128_t{owner.value} << 64) | tag.value; }
      //uint64_t by_expiration() const { return expiration.sec_since_epoch(); }
    };
    typedef multi_index<"cronjobs"_n, cronjobs,
    eosio::indexed_by<"byowner"_n, eosio::const_mem_fun<cronjobs, uint64_t, &cronjobs::by_owner>>,
    eosio::indexed_by<"byduedate"_n, eosio::const_mem_fun<cronjobs, uint64_t, &cronjobs::by_due_date>>,
    indexed_by<"byownertag"_n, const_mem_fun<cronjobs, uint128_t, &cronjobs::by_owner_tag>>
    //,eosio::indexed_by<"byexpiration"_n, eosio::const_mem_fun<cronjobs, uint64_t, &cronjobs::by_expiration>>
    
    > cronjobs_table;
  //************************


    //************************
    TABLE gastokens {
      extended_asset token;
      uint64_t primary_key() const { return token.get_extended_symbol().get_symbol().raw(); }// primary key is symbol
      uint128_t by_cntr_sym() const { return (uint128_t{token.contract.value} << 64) | token.quantity.symbol.raw(); };
    };
    typedef multi_index<"gastokens"_n, gastokens,
      indexed_by<"bycntrsym"_n, const_mem_fun<gastokens, uint128_t, &gastokens::by_cntr_sym>>
    > gastokens_table;

  //************************

  //************************
    //scoped table
    TABLE deposits {
      asset balance;
      uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };
    typedef multi_index<"deposits"_n, deposits> deposits_table;

  //************************

  //************************
    //scoped table
    TABLE rewards {
      asset adj_p_balance;
      uint64_t primary_key()const { return adj_p_balance.symbol.code().raw(); }
    };
    typedef multi_index<"rewards"_n, rewards> rewards_table;

  //************************
  
  //************************  
  TABLE settings {
    uint8_t max_allowed_actions;
    vector<permission_level> required_exec_permission;
    uint8_t reward_fee_perc;
    asset new_scope_fee;
    name token_contract;
  };
  typedef eosio::singleton<"settings"_n, settings> settings_table;
  //************************

  //************************  
  TABLE state {
    uint64_t schedule_count;
    uint64_t exec_count;
    uint64_t cancel_count;
    uint64_t expired_count;
  };
  typedef eosio::singleton<"state"_n, state> state_table;
  //************************

  //************************
  TABLE execaccounts {
    uint64_t id;
    name owner;
    permission_level exec_permission;

    uint64_t primary_key() const { return id; }
    uint64_t by_owner() const { return owner.value; }
    uint64_t by_acc() const { return exec_permission.actor.value; }
    //uint128_t by_perm() const { return (uint128_t{exec_permission.actor.value} << 64) | exec_permission.permission.value; };
  };
  typedef multi_index<"execaccounts"_n, execaccounts,
    indexed_by<"byowner"_n, const_mem_fun<execaccounts, uint64_t, &execaccounts::by_owner>>,
    indexed_by<"byacc"_n, const_mem_fun<execaccounts, uint64_t, &execaccounts::by_acc>>
    //indexed_by<"byperm"_n, const_mem_fun<execaccounts, uint128_t, &execaccounts::by_perm>>
  > execaccounts_table;
  //************************

  //************************
  TABLE trusteddapps {
    name contract;
    string description;
    string url;
    string logo;
    uint64_t approved = 0;

    uint64_t primary_key() const { return contract.value; }
    uint64_t by_approved() const { return approved; }
  };
  typedef multi_index<"trusteddapps"_n, trusteddapps,
    indexed_by<"byapproved"_n, const_mem_fun<trusteddapps, uint64_t, &trusteddapps::by_approved>>
  > trusteddapps_table;
  //************************

  //************************
  TABLE privscopes {
    name scope;
    name owner;
    scope_meta meta;
    uint64_t primary_key() const { return scope.value; }
    uint64_t by_owner() const { return owner.value; }
  };
  typedef multi_index<"privscopes"_n, privscopes,
    indexed_by<"byowner"_n, const_mem_fun<privscopes, uint64_t, &privscopes::by_owner>>
  > privscopes_table;
  //************************

  //************************
  //scoped by private scope name
  TABLE scopeusers {
    name user;
    uint64_t primary_key() const { return user.value; }
  };
  typedef multi_index<"scopeusers"_n, scopeusers > scopeusers_table;
  //************************


  //************************
  //functions
  //************************
  uint64_t get_next_primary_key();

  void assert_invalid_authorization(vector<permission_level> auths, const settings& setting);
  //void assert_blacklisted_actionname(name actionname);
  bool is_valid_fee_token(name tokencontract, asset quantity);
  bool is_valid_fee_symbol(symbol& symbol);
  auto get_settings();
  void sub_balance( const name& owner, const asset& value);
  void add_balance( const name& owner, const asset& value);
  void sub_reward( const name& miner,  asset value);
  void add_reward( const name& miner,  asset value, const settings& setting);

  name get_contract_for_symbol(symbol sym);

  bool has_scope_write_access(const name&  user, const name& scope);

  //bool clean_expired(cronjobs_table& idx, uint32_t batch_size);

  bool is_master_authorized_to_use_slave(const permission_level& master, const permission_level& slave){
    vector<permission_level> masterperm = { master };
    auto packed_master = pack(masterperm);
    auto test = eosio::check_permission_authorization(
                slave.actor,
                slave.permission,
                (const char *) 0, 0,
                packed_master.data(),
                packed_master.size(), 
                microseconds(0)
    );
    return test > 0 ? true : false;
  };


  template <typename T>
  void cleanTable(name code, uint64_t account, const uint32_t batchSize){
    T db(code, account);
    uint32_t counter = 0;
    auto itr = db.begin();
    while(itr != db.end() && counter++ < batchSize) {
        itr = db.erase(itr);
    }
  }

};
