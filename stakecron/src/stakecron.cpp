#include <stakecron.hpp>
ACTION stakecron::setstaketype(name staketype, extended_asset stake_token, uint64_t release_sec,  bool enable){
  require_auth(get_self() );
  staketypes_table _staketypes(get_self(), get_self().value);
  auto itr = _staketypes.find(staketype.value);
  if(itr == _staketypes.end() ){
    _staketypes.emplace(get_self(), [&](auto &n) {
      n.staketype = staketype;
      n.stake_token = stake_token;
      n.release_sec = release_sec;
      n.enable = enable;
    });  
  }
  else{
    _staketypes.modify(itr, same_payer, [&]( auto& n) {
      n.stake_token = stake_token;
      n.release_sec = release_sec;
      n.enable = enable;
    });
  }

}
ACTION stakecron::rmstaketype(name staketype){
  require_auth(get_self());
  staketypes_table _staketypes(get_self(), get_self().value);
  auto itr = _staketypes.find(staketype.value);
  check(itr != _staketypes.end(), "Staketype doesn't exists.");
  _staketypes.erase(itr);
}

ACTION stakecron::open(name staker, name ram_payer, name staketype) {
  require_auth(ram_payer);
  staketypes s = get_stake_type(staketype);//this will assert if type doesn't exists
  stakes_table _stakes(get_self(), staketype.value);
  auto itr = _stakes.find(staker.value);

  if(itr == _stakes.end() ){
    
    _stakes.emplace(ram_payer, [&](auto &n) {
      n.staker = staker;
      n.amount = asset(0, s.stake_token.quantity.symbol);
    });  
  
  }
}

ACTION stakecron::unstake(name staker, asset amount, name staketype) {
  require_auth(staker);
  check(amount.amount > 0, "Unstake amount must be greater then zero.");
  staketypes s = get_stake_type(staketype);
  check(amount.symbol == s.stake_token.quantity.symbol, "Symbol does not match the stake token for type "+staketype.to_string()+".");
  stakes_table _stakes(get_self(), staketype.value);
  auto itr = _stakes.find(staker.value);
  check(itr != _stakes.end(), "Account not found in stakes table for type "+staketype.to_string()+".");

  check(amount.amount <= itr->amount.amount, "Unstake amount greater then amount staked.");

  //deduct unstake amount
  _stakes.modify(itr, same_payer, [&]( auto& n) {
        n.amount -= amount;
  });

  //add to unstake table
  unstakes_table _unstakes(get_self(), get_self().value );
  _unstakes.emplace(staker, [&](auto &n) {
    n.id = _unstakes.available_primary_key();
    n.staker = staker;
    n.amount = amount;
    n.staketype = staketype;
    n.release_date = time_point_sec(time_point_sec(current_time_point() ).sec_since_epoch() + s.release_sec );
  }); 


}

ACTION stakecron::withdraw(name staker, uint64_t id) {
  require_auth(staker);
  
  unstakes_table _unstakes(get_self(), get_self().value);
  auto unstake_entry = _unstakes.find(id);
  check(unstake_entry != _unstakes.end(), "Unstake with this id doesn't exists");
  time_point_sec now = time_point_sec(current_time_point() );
  check(unstake_entry->staker == staker, "Initiater is not the owner of the unstake.");
  check(now >= unstake_entry->release_date, "Unstake not mature yet. Wait "+to_string(unstake_entry->release_date.sec_since_epoch()-now.sec_since_epoch()) +"sec.");

  staketypes s = get_stake_type(unstake_entry->staketype);

  action(
      permission_level{get_self(), "active"_n},
      s.stake_token.contract, "transfer"_n,
      make_tuple( get_self(), staker, unstake_entry->amount, string("Refund mature unstaked funds.") )
  ).send();

  _unstakes.erase(unstake_entry);
}

ACTION stakecron::close(name staker, name staketype) {
  require_auth(staker);
  stakes_table _stakes(get_self(), staketype.value);
  auto itr = _stakes.find(staker.value);
  check(itr !=  _stakes.end(), "Staker doesn't have an open account for this type.");
  check(itr->amount.amount == 0, "Can't close stake account with positive balance. Unstake first.");
  _stakes.erase(itr);
}


//notify transfer handler
void stakecron::on_transfer(name from, name to, asset quantity, string memo){
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

  name staketype = name(memo.substr(0, 12) );
  staketypes_table _staketypes(get_self(), get_self().value );
  auto existing = _staketypes.find(staketype.value);
  if(existing == _staketypes.end() ){
    //allow normal transfers
    return;
  }
  staketypes s = *existing;

  check(get_first_receiver() == s.stake_token.contract, "Token contract not allowed.");
  check(quantity.symbol == s.stake_token.quantity.symbol, "Symbol not allowed.");

  stakes_table _stakes(get_self(), staketype.value);
  auto itr = _stakes.find(from.value);
  check(itr != _stakes.end(), "Use the open action to create a stake account.");
  _stakes.modify(itr, same_payer, [&]( auto& n) {
        n.amount += quantity;
  });

}

stakecron::staketypes stakecron::get_stake_type(const name& staketype){
  staketypes_table _staketypes(get_self(), get_self().value);
  return _staketypes.get(staketype.value, "Staketype doesn't exists.");
}



