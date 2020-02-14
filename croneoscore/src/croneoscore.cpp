#include <croneoscore.hpp>
#include <functions.cpp>
#include <dev.cpp>

ACTION croneoscore::setsettings(uint8_t max_allowed_actions, vector<permission_level> required_exec_permission, uint8_t reward_fee_perc, asset new_scope_fee, name token_contract){
    require_auth(get_self());
    settings_table _settings(get_self(), get_self().value);
    _settings.set(settings{
      max_allowed_actions, 
      required_exec_permission,
      reward_fee_perc,
      new_scope_fee, 
      token_contract
    }, get_self());
}


ACTION croneoscore::schedule(
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
){
  require_auth(owner);

  scope = scope == name(0) ? get_self() : scope;
  if(scope != get_self() ){
    check(has_scope_write_access(owner, scope), "User "+owner.to_string()+" is not authorized to schedule in "+scope.to_string()+" scope.");
  }
  uint8_t max_exec_count = 1;//todo must be action argument
  //validate and handle gas_fee
  check(is_valid_fee_symbol(gas_fee.symbol), "CRONEOS::ERR::001:: Symbol not allowed for paying gas.");
  check(gas_fee.amount >= 0, "CRONEOS::ERR::002:: gas fee can't be negative.");
  if(gas_fee.amount > 0){
    if(max_exec_count == 1){
      sub_balance(owner, gas_fee);
    }
    else{
     check(max_exec_count > 0, "max_exec_count must be greater then zero.");
     sub_balance(owner, gas_fee*max_exec_count); 
    }
  }
  else{
    //no gas payed, restrict max_exec_count
    check(max_exec_count == 1, "Max exec count must be 1 for jobs without gas fee.");
  }

  time_point_sec now = time_point_sec(current_time_point());
  /////////// time conversions and checks //////////////
  time_point_sec exec_date;
  exec_date = due_date == time_point_sec(0) ? time_point_sec(now.sec_since_epoch() + delay_sec) : due_date;

  time_point_sec expiration_date;
  expiration_date = expiration == time_point_sec(0) ? time_point_sec(exec_date.sec_since_epoch() + expiration_sec) : expiration;

  check(exec_date >= now, "CRONEOS::ERR::003:: Execution date can't be earlier then current time.");
  check(expiration_date > exec_date, "CRONEOS::ERR::004:: The expiration date must be later then the execution date.");
  ////////////end time stuff////////////

  settings setting = get_settings();

  /////validate scheduled actions////////
  check(actions.size() <= setting.max_allowed_actions && actions.size() > 0, "CRONEOS::ERR::005:: Number of actions not allowed.");

  for ( auto i = actions.begin(); i != actions.end(); i++ ) {
    assert_blacklisted_account(i->account);
    assert_blacklisted_actionname(i->name);
    assert_invalid_authorization(i->authorization, setting);
  }
  /////end validate scheduled actions////////

  //////tag based logic//////
  
  cronjobs_table _cronjobs(get_self(), scope.value);

  if(tag != name(0) ){
    auto by_owner_and_tag = _cronjobs.get_index<"byownertag"_n>();
    uint128_t composite_id = (uint128_t{owner.value} << 64) | tag.value;
    auto job_itr = by_owner_and_tag.find(composite_id);

    if(job_itr != by_owner_and_tag.end() ){

      if( job_itr->expiration < now ){//job with tag expired

        if(job_itr->gas_fee.amount > 0){
          add_balance( job_itr->owner, job_itr->gas_fee);//refund gas fee
        }
        by_owner_and_tag.erase(job_itr);
        //should modify this row instead of erasing and adding it back?

      }
      else{
        check(false, "CRONEOS::ERR::005:: Duplicate tag for owner. Only expired jobs can be replaced.");
      }    
    }

  }
  //////end based tag logic//////

  ////all checks and processing done -> insert in to cronjobs table///
  _cronjobs.emplace(owner, [&](auto& n) {
    n.id = _cronjobs.available_primary_key();
    n.owner = owner;
    n.tag = tag;
    n.auth_bouncer = auth_bouncer;
    n.actions = actions;
    n.due_date = exec_date;
    n.expiration = expiration_date; 
    n.submitted = now;
    n.gas_fee = gas_fee;// can be zero
    n.description = description;//optional
    n.oracle_srcs = oracle_srcs;
  });
}

ACTION croneoscore::cancel(name owner, uint64_t id, name scope){
    require_auth(owner);
    scope = scope == name(0) ? get_self() : scope;
    cronjobs_table _cronjobs(get_self(), scope.value);
    auto jobs_itr = _cronjobs.find(id);
    check(jobs_itr != _cronjobs.end(), "CRONEOS::ERR::006:: Scheduled action with this id doesn't exists in "+ scope.to_string()+" scope.");
    check(jobs_itr->owner == owner, "CRONEOS::ERR::007:: You are not the owner of this job.");
    if(jobs_itr->gas_fee.amount > 0){
      //todo exec_count (left over) * gas fee
      add_balance( jobs_itr->owner, jobs_itr->gas_fee);//refund gas fee
    }
    _cronjobs.erase(jobs_itr);
}

ACTION croneoscore::execoracle(name executer, uint64_t id, std::vector<char> oracle_response, name scope){
  require_auth(executer);
  scope = scope == name(0) ? get_self() : scope;
  cronjobs_table _cronjobs(get_self(), scope.value);
  auto jobs_itr = _cronjobs.find(id);
  check(jobs_itr != _cronjobs.end(), "CRONEOS::ERR::006:: Scheduled action with this id doesn't exists in "+ scope.to_string()+" scope.");

  //send-execute scheduled actions
  for(vector<int>::size_type i = 0; i != jobs_itr->actions.size(); i++) { 
      eosio::action act = jobs_itr->actions[i];
      act.data = oracle_response;
      act.send();
  }
  //? can do this in the exec action... no need for separate execoracle ??
  //get job by id
  //auth guard
  //check if oracle_srcs isn't empty
  //check if due date met
  //check if not expired
  //substitute job action data with oracle_response
  //send
  //delete job and pay miner
}

ACTION croneoscore::exec(name executer, uint64_t id, name scope){
  require_auth(executer);
  scope = scope == name(0) ? get_self() : scope;
  cronjobs_table _cronjobs(get_self(), scope.value);
  auto jobs_itr = _cronjobs.find(id);
  check(jobs_itr != _cronjobs.end(), "CRONEOS::ERR::006:: Scheduled action with this id doesn't exists in "+ scope.to_string()+" scope.");

  //require auth from guard account
  if(jobs_itr->auth_bouncer != name(0) ){
    require_auth(jobs_itr->auth_bouncer);
  }

  time_point_sec now = time_point_sec(current_time_point());
  //check if job is expired
  if(jobs_itr->expiration < now){
    
    if(jobs_itr->gas_fee.amount > 0){
      add_balance( jobs_itr->owner, jobs_itr->gas_fee);//refund gas fee
      //todo pay small cron reward for deleting expired
    }
    
    _cronjobs.erase(jobs_itr);
    return;
  }

  //assert when job isn't ready to be executed
  check( now >= jobs_itr->due_date , "CRONEOS::ERR::007:: Exec attempt too early.");

  //send-execute scheduled actions
  for(vector<int>::size_type i = 0; i != jobs_itr->actions.size(); i++) { 
      jobs_itr->actions[i].send();
  }

  settings setting = get_settings();
  //payout rewards 
  if(jobs_itr->gas_fee.amount > 0){
    //todo payout CRON
    //todo only part of gas fee
    add_reward(executer, jobs_itr->gas_fee, setting);
  }

  //_cronjobs.erase(jobs_itr);

  if(jobs_itr->max_exec_count == 1){
    //this was the last execution so erase
    _cronjobs.erase(jobs_itr);
  }
  else{
    _cronjobs.modify( jobs_itr, same_payer, [&]( auto& n) {
        n.max_exec_count--;
    });
  }

}

ACTION croneoscore::addblacklist(name contract){
  require_auth(get_self());
  blacklist_table _blacklist(get_self(), get_self().value);
  auto blacklist_itr = _blacklist.find(contract.value);
  check(blacklist_itr == _blacklist.end(), "CRONEOS::ERR::008:: Account already blacklisted.");
  check(is_account(contract), "CRONEOS::ERR::009:: The account doesn't exists.");

  _blacklist.emplace(get_self(), [&](auto& n) {
    n.contract = contract;
  });
}
ACTION croneoscore::rmblacklist(name contract){
  require_auth(get_self());
  blacklist_table _blacklist(get_self(), get_self().value);
  auto blacklist_itr = _blacklist.find(contract.value);
  check(blacklist_itr != _blacklist.end(), "CRONEOS::ERR::010:: Account not in blacklist.");
  _blacklist.erase(blacklist_itr);
}

ACTION croneoscore::addgastoken(extended_asset gas_token){
  //we don't allow tokens with the same symbol, even if from other contract!
  require_auth(get_self());
  gastokens_table _gastokens(get_self(), get_self().value);
  auto token_itr = _gastokens.find(gas_token.quantity.symbol.raw() );
  check(token_itr == _gastokens.end(), "CRONEOS::ERR::011:: Token with this symbol already listed as gas token.");
  check(is_account(gas_token.contract), "CRONEOS::ERR::009:: The account doesn't exists.");

  _gastokens.emplace(get_self(), [&](auto& n) {
    n.token = gas_token;
  });
}

ACTION croneoscore::rmgastoken(asset gas_token){
  require_auth(get_self());
  gastokens_table _gastokens(get_self(), get_self().value);
  auto token_itr = _gastokens.find(gas_token.symbol.raw() );
  check(token_itr != _gastokens.end(), "CRONEOS::ERR::012:: Token with this symbol doesn't exist.");
  _gastokens.erase(token_itr);
}



ACTION croneoscore::refund(name owner, asset amount) {
  //refund ~= withdrawal from gas deposits
  require_auth(owner);
  check(amount.amount > 0, "CRONEOS::ERR::013:: Amount must be greater then zero.");
  sub_balance( owner, amount);
  action(
    permission_level{get_self(), "active"_n},
    get_contract_for_symbol(amount.symbol), "transfer"_n,
    make_tuple(get_self(), owner, amount, string("Refund deposit."))
  ).send();
}

ACTION croneoscore::withdraw( name miner, asset amount){
  require_auth(miner);
  check(amount.amount > 0, "CRONEOS::ERR::013:: Amount must be greater then zero.");
  sub_reward( miner, amount);
  action(
    permission_level{get_self(), "active"_n},
    get_contract_for_symbol(amount.symbol), "transfer"_n,
    make_tuple(get_self(), miner, amount, string("Mining payout."))
  ).send();
}

ACTION croneoscore::setprivscope (name actor, name owner, name scope, bool remove){
  require_auth(actor);
  privscopes_table _privscopes(get_self(), get_self().value);
  auto priv_itr = _privscopes.find(scope.value);

  if(priv_itr != _privscopes.end() ){
    //existing priv scope
    check(actor == priv_itr->owner, "You are not the owner, private scope "+scope.to_string()+" already taken.");
    if(remove){
      scopeusers_table _scopeusers(get_self(), scope.value);
      check(_scopeusers.begin() == _scopeusers.end(), "Please remove the scope users before deleting.");
      _privscopes.erase(priv_itr);
      return;
    }
    check(priv_itr->owner != owner, owner.to_string()+" is already owner/admin of this scope.");
    check(is_account(owner), "New owner isn't an existing account.");
    _privscopes.modify( priv_itr, same_payer, [&]( auto& n) {
        n.owner = owner;//new  private scope owner/admin
    });
  }
  else{
    //new priv scope
    check(!remove, "Remove flag can't be true.");
    check(scope != get_self() && scope != name(0) && scope != name("croneos"), "Scope name not allowed." );
    check(is_account(owner), "Owner isn't an existing account.");
    if(actor != get_self()){
      if(get_settings().new_scope_fee.amount > 0){
        sub_balance( actor, get_settings().new_scope_fee);     
      }
    }
    _privscopes.emplace(actor, [&](auto& n) {
      n.scope = scope;
      n.owner = owner;
    });

  }
}
ACTION croneoscore::setscopemeta (name owner, name scope, scope_meta meta){
  require_auth(owner);
  privscopes_table _privscopes(get_self(), get_self().value);
  auto priv_itr = _privscopes.find(scope.value);
  check(priv_itr != _privscopes.end(), "Private scope doesn't exist." );
  check(priv_itr->owner == owner, "You are not the owner/admin of this scope.");
  _privscopes.modify( priv_itr, same_payer, [&]( auto& n) {
    n.meta = meta;
  });
}

ACTION croneoscore::setscopeexec (name owner, name scope, vector<permission_level> required_exec_permissions){
  require_auth(owner);
  privscopes_table _privscopes(get_self(), get_self().value);
  auto priv_itr = _privscopes.find(scope.value);
  check(priv_itr != _privscopes.end(), "Private scope doesn't exist." );
  check(priv_itr->owner == owner, "You are not the owner/admin of this scope.");


  _privscopes.modify( priv_itr, same_payer, [&]( auto& n) {
    n.required_exec_permissions = required_exec_permissions;
  });


  //check(test > 0, get_self().to_string()+"@active"+" doesn't have permission to use "+required_exec_permissions[0].actor.to_string()+"@"+required_exec_permissions[0].permission.to_string() );

}

ACTION croneoscore::setscopeuser (name owner, name scope, name user, bool remove){
  require_auth(owner);
  privscopes_table _privscopes(get_self(), get_self().value);
  auto priv = _privscopes.get(scope.value, "Private scope doesn't exist.");
  check(priv.owner == owner, "You are not the owner/admin of this scope.");

  scopeusers_table _scopeusers(get_self(), scope.value);
  auto itr_scopeusr = _scopeusers.find(user.value);
  if(itr_scopeusr != _scopeusers.end() ){//existing user
    if(remove){
      _scopeusers.erase(itr_scopeusr);
      return;
    }
    check(false, user.to_string()+ " is already a user from this scope.");
  }
  else{//new user
    check(is_account(user), "User isn't an existing account.");
    _scopeusers.emplace(owner, [&](auto& n) {
      n.user = user;
    });
  }
}


//notify transfer handler
void croneoscore::top_up_balance(name from, name to, asset quantity, string memo){
  require_auth(from);

  if (from == get_self() || to != get_self()) {
    return;
  }

  if ( from == name("eosio") || from == name("eosio.bpay") ||
       from == name("eosio.msig") || from == name("eosio.names") ||
       from == name("eosio.prods") || from == name("eosio.ram") ||
       from == name("eosio.ramfee") || from == name("eosio.saving") ||
       from == name("eosio.stake") || from == name("eosio.token") ||
       from == name("eosio.unregd") || from == name("eosio.vpay") ||
       from == name("eosio.wrap") || from == name("eosio.rex") ) {
    return;
  }
  //need to double check memo parsing!!!
  //!!!!
  if(memo.substr(0, 7) != "deposit" ){
    return; //accept normal transfers
  }

  check(is_valid_fee_token(get_first_receiver(), quantity), "CRONEOS::ERR::014:: Invalid gas deposit, token not supported.");

  name receiving_owner = from;
  
  if(memo[7] == ':' ){
    string potentialaccountname = memo.length() >= 8 ? memo.substr(8, 12 ) : "";
    //todo check if the receiver is a cron utilizer
    check(is_account(name(potentialaccountname)), "CRONEOS::ERR::015:: invalid receiver account supplied via memo.");
    receiving_owner = name(potentialaccountname);
  }

  add_balance( receiving_owner, quantity);
}






