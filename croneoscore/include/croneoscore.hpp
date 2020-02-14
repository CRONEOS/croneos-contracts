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
    ACTION cancel(name owner, uint64_t id, name scope);
    ACTION exec(name executer, uint64_t id, name scope);
    ACTION execoracle(name executer, uint64_t id, std::vector<char> oracle_response, name scope);

    ACTION addblacklist(name contract);
    ACTION rmblacklist(name contract);
    ACTION setsettings(uint8_t max_allowed_actions, vector<permission_level> required_exec_permission, uint8_t reward_fee_perc, asset new_scope_fee, name token_contract);

    ACTION withdraw( name miner, asset amount );
    ACTION refund(name owner, asset amount);

    ACTION setprivscope (name actor, name scope_owner, name scope, bool remove);
    ACTION setscopeexec (name owner, name scope, vector<permission_level> required_exec_permissions);
    ACTION setscopemeta (name owner, name scope, scope_meta meta);
    ACTION setscopeuser (name owner, name scope, name user, bool remove);
    

    ACTION addgastoken(extended_asset gas_token);
    ACTION rmgastoken(asset gas_token);

    ACTION delrewards(name scope);
    ACTION delsettings();
    ACTION clear();
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
      //vector <oracle_src> oracle_srcs;
      
      uint64_t primary_key() const { return id; }
      uint64_t by_owner() const { return owner.value; }
      uint64_t by_due_date() const { return due_date.sec_since_epoch(); }
      uint128_t by_owner_tag() const { return (uint128_t{owner.value} << 64) | tag.value; }
    };
    typedef multi_index<"cronjobs"_n, cronjobs,
    eosio::indexed_by<"byowner"_n, eosio::const_mem_fun<cronjobs, uint64_t, &cronjobs::by_owner>>,
    eosio::indexed_by<"byduedate"_n, eosio::const_mem_fun<cronjobs, uint64_t, &cronjobs::by_due_date>>,
    indexed_by<"byownertag"_n, const_mem_fun<cronjobs, uint128_t, &cronjobs::by_owner_tag>>
    
    > cronjobs_table;
  //************************

  //************************
    TABLE blacklist {
      name contract;
      auto primary_key() const { return contract.value; }
    };
    typedef multi_index<"blacklist"_n, blacklist> blacklist_table;

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
  TABLE privscopes {
    name scope;
    name owner;
    vector<permission_level> required_exec_permissions;//if set this will override the default required_exec_permission
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
  void assert_invalid_authorization(vector<permission_level> auths, const settings& setting);
  void assert_blacklisted_account(name account);
  void assert_blacklisted_actionname(name actionname);
  bool is_valid_fee_token(name tokencontract, asset quantity);
  bool is_valid_fee_symbol(symbol& symbol);
  auto get_settings();
  void sub_balance( const name& owner, const asset& value);
  void add_balance( const name& owner, const asset& value);
  void sub_reward( const name& miner,  asset value);
  void add_reward( const name& miner,  asset value, const settings& setting);
  name get_contract_for_symbol(symbol sym);

  bool has_scope_write_access(const name&  user, const name& scope);

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
