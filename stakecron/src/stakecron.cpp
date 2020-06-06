#include <stakecron.hpp>
ACTION stakecron::setsettings(settings new_settings){
  require_auth(get_self() );
  settings_table _settings(get_self(), get_self().value);
  _settings.set(new_settings, get_self() );

}
ACTION stakecron::open(name staker, name ram_payer) {
  require_auth(ram_payer);
  stakes_table _stakes(get_self(), get_self().value);
  auto itr = _stakes.find(staker.value);

  if(itr == _stakes.end() ){
    settings_table _settings(get_self(), get_self().value);
    settings s = _settings.get();
    _stakes.emplace(ram_payer, [&](auto &n) {
      n.staker = staker;
      n.amount = asset(0, s.stake_token.quantity.symbol);
    });  
  
  }
}

ACTION stakecron::unstake(name staker, asset amount) {
  require_auth(staker);
  check(amount.amount > 0, "Unstake amount must be greater then zero.");
  settings_table _settings(get_self(), get_self().value);
  settings s = _settings.get();
  check(amount.symbol == s.stake_token.quantity.symbol, "Symbol does not match the stake token.");
  stakes_table _stakes(get_self(), get_self().value);
  auto itr = _stakes.find(staker.value);
  check(itr != _stakes.end(), "Account not found in stakes table.");

  check(amount.amount <= itr->amount.amount, "Unstake amount greater then amount staked.");

  //deduct unstake amount
  _stakes.modify(itr, same_payer, [&]( auto& n) {
        n.amount -= amount;
  });

  //add to unstake table
  unstakes_table _unstakes(get_self(), get_self().value);
  _unstakes.emplace(staker, [&](auto &n) {
    n.id = _unstakes.available_primary_key();
    n.staker = staker;
    n.amount = amount;
    n.release_date = time_point_sec(time_point_sec(current_time_point() ).sec_since_epoch() + s.release_sec );
  }); 


}

ACTION stakecron::withdraw(name staker, uint64_t id) {
  require_auth(staker);
  unstakes_table _unstakes(get_self(), get_self().value);
  auto unstake_entry = _unstakes.get(id);
  time_point_sec now = time_point_sec(current_time_point() );
  check(unstake_entry.staker == staker, "Initiater is not the owner of the unstake.");
  check(now >= unstake_entry.release_date, "Unstake not mature yet.");

  settings_table _settings(get_self(), get_self().value);
  settings s = _settings.get();

  action(
      permission_level{get_self(), "active"_n},
      s.stake_token.contract, "transfer"_n,
      make_tuple( get_self(), staker, unstake_entry.amount, string("Refund mature unstaked funds.") )
  ).send();

  _unstakes.erase(unstake_entry);
}

ACTION stakecron::close(name staker) {
  require_auth(staker);
  stakes_table _stakes(get_self(), get_self().value);
  auto existing_staker = _stakes.get(staker.value);
  check(existing_staker.amount.amount == 0, "Can't close stake account with positive balance. Unstake first.");
  _stakes.erase(existing_staker);
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

  if(memo.substr(0, 5) != "stake" ){
    return; //accept normal transfers
  }

  settings_table _settings(get_self(), get_self().value);
  settings s = _settings.get();
  check(get_first_receiver() == s.stake_token.contract, "Token contract not allowed");
  check(quantity.symbol == s.stake_token.quantity.symbol, "Symbol not allowed");

  stakes_table _stakes(get_self(), get_self().value);
  auto itr = _stakes.find(from.value);
  check(itr != _stakes.end(), "Use the open action to create a stake account");
  _stakes.modify(itr, same_payer, [&]( auto& n) {
        n.amount += quantity;
  });

}



