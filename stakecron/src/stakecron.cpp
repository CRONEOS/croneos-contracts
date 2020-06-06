#include <stakecron.hpp>

ACTION stakecron::open(name staker) {
  require_auth(staker);
}

ACTION stakecron::unstake(name staker, asset amount) {
  require_auth(staker);
}

ACTION stakecron::withdraw(name staker, uint64_t id) {
  require_auth(staker);
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
  //need to double check memo parsing!!!
  //!!!!
  if(memo.substr(0, 7) != "deposit" ){
    return; //accept normal transfers
  }

}

/*
ACTION stakecron::clear() {
  require_auth(get_self());

  messages_table _messages(get_self(), get_self().value);

  // Delete all records in _messages table
  auto msg_itr = _messages.begin();
  while (msg_itr != _messages.end()) {
    msg_itr = _messages.erase(msg_itr);
  }
}
*/


