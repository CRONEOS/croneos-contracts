#include <croneoscore.hpp>
#include <functions.cpp>
#include <dev.cpp>

ACTION croneoscore::setsettings(uint8_t max_allowed_actions, vector<permission_level> required_exec_permission, uint8_t reward_fee_perc, name token_contract){
    require_auth(get_self());
    settings_table _settings(get_self(), get_self().value);
    _settings.set(settings{
      max_allowed_actions, 
      required_exec_permission,
      reward_fee_perc, 
      token_contract
    }, get_self());
}


ACTION croneoscore::schedule(name owner, name tag, vector<action> actions, time_point_sec due_date, uint32_t delay_sec, time_point_sec expiration, uint32_t expiration_sec, asset gas_fee, string description ) {
  require_auth(owner);

  //validate and handle gas_fee
  check(is_valid_fee_symbol(gas_fee.symbol), "CRONEOS::ERR::001:: Symbol not allowed for paying gas.");
  check(gas_fee.amount >= 0, "CRONEOS::ERR::002:: gas fee can't be negative.");
  if(gas_fee.amount > 0){
    sub_balance(owner, gas_fee);
  }

  /////////// time conversions and checks //////////////
  time_point_sec exec_date;
  exec_date = delay_sec == 0 ? due_date : time_point_sec(current_time_point().sec_since_epoch() + delay_sec);

  time_point_sec expiration_date;
  expiration_date = expiration_sec == 0 ? expiration : time_point_sec(exec_date.sec_since_epoch() + expiration_sec);

  check(exec_date > time_point_sec(current_time_point()), "CRONEOS::ERR::003:: Execution date can't be earlier then current time.");
  check(expiration_date > exec_date, "CRONEOS::ERR::004:: The expiration date must be later then the execution date.");
  ////////////end time stuff////////////

  settings setting = get_settings();

  /////validate scheduled actions////////
  check(actions.size() <= setting.max_allowed_actions && actions.size() > 0, "CRONEOS::ERR::005:: Number of combined actions not allowed.");

  for ( auto i = actions.begin(); i != actions.end(); i++ ) {
    assert_blacklisted_account(i->account);
    assert_blacklisted_actionname(i->name);
    assert_invalid_authorization(i->authorization, setting);
  }
  /////end validate scheduled actions////////

  //////tag based logic//////
  time_point_sec now = time_point_sec(current_time_point());
  cronjobs_table _cronjobs(get_self(), get_self().value);

  if(tag != name(0) ){
    auto by_owner_and_tag = _cronjobs.get_index<"byownertag"_n>();
    uint128_t composite_id = (uint128_t{owner.value} << 64) | tag.value;
    auto job_itr = by_owner_and_tag.find(composite_id);

    if(job_itr != by_owner_and_tag.end() ){//only do this when owner has jobs
      if( job_itr->expiration < now ){

        if(job_itr->gas_fee.amount > 0){
          add_balance( job_itr->owner, job_itr->gas_fee);//refund gas fee
        }
        by_owner_and_tag.erase(job_itr);
        //should modify this row instead of erasing and adding it back. 

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
    n.actions = actions;
    n.due_date = exec_date;
    n.expiration = expiration_date; 
    n.submitted = now;
    n.gas_fee = gas_fee;// can be zero
    n.description = description;//optional
  });
}

ACTION croneoscore::cancel(name owner, uint64_t id ){
    require_auth(owner);
    cronjobs_table _cronjobs(get_self(), get_self().value);
    auto jobs_itr = _cronjobs.find(id);
    check(jobs_itr != _cronjobs.end(), "CRONEOS::ERR::006:: Scheduled action with this id doesn't exists.");
    check(jobs_itr->owner == owner, "CRONEOS::ERR::007:: You are not the owner of this job.");
    if(jobs_itr->gas_fee.amount > 0){
      add_balance( jobs_itr->owner, jobs_itr->gas_fee);//refund gas fee
    }
    _cronjobs.erase(jobs_itr);
}

ACTION croneoscore::exec(name executer, uint64_t id){
  require_auth(executer);
  cronjobs_table _cronjobs(get_self(), get_self().value);
  auto jobs_itr = _cronjobs.find(id);
  check(jobs_itr != _cronjobs.end(), "CRONEOS::ERR::006:: Scheduled action with this id doesn't exists.");

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

  //update cronjobs table: should delete entry, make it optional?
  _cronjobs.erase(jobs_itr);

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
  //we don't allow tokens with the same symbol, even if from other contract.
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

  if(memo.substr(0, 11) != "deposit gas" ){
    return; //accept normal transfers
  }

  check(is_valid_fee_token(get_first_receiver(), quantity), "CRONEOS::ERR::014:: Invalid gas deposit, token not supported.");

  name receiving_owner = from;
  
  if(memo[11] == ':' ){
    string potentialaccountname = memo.length() >= 12 ? memo.substr(12, 12 ) : "";
    //todo check if the receiver is a cron utilizer
    check(is_account(name(potentialaccountname)), "CRONEOS::ERR::015:: invalid receiver account supplied via memo.");
    receiving_owner = name(potentialaccountname);
  }

  add_balance( receiving_owner, quantity);
}
















