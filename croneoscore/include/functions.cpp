uint64_t croneoscore::get_next_primary_key(){
  state_table _state(get_self(), get_self().value);
  auto s = _state.get_or_create(get_self(), state());
  s.schedule_count = s.schedule_count + 1;
  _state.set(s, get_self());
  return s.schedule_count;
};

auto croneoscore::get_settings(){
  settings_table _settings(get_self(), get_self().value);
  auto setting = _settings.get();
  return setting;
} 

void croneoscore::assert_invalid_authorization(vector<permission_level> auths, const settings& setting){
    bool has_required_auth = false;
    permission_level required_permission = setting.required_exec_permission[0]; // execexecexec@active -> todo use custom for scope

    for ( auto i = auths.begin(); i != auths.end(); i++ ) {

        if(i->actor == required_permission.actor && i->permission == required_permission.permission ){
            has_required_auth = true;
        }
        else{
            //assert if actor is self and if actor is required_permission.actor BUT NOT with correct permission (ie active)
            check(i->actor != get_self() && i->actor != required_permission.actor, "CRONEOS::ERR::017:: Not authorized to use this permission.");
        }
        
    }
    check(has_required_auth, "CRONEOS::ERR::018:: Scheduled actions must be authorized with the required_exec_permission.");//see settings table
}

/*
void croneoscore::assert_blacklisted_actionname(name actionname){
  check(
    actionname != "transfer"_n
    || actionname != "issue"_n,

    "CRONEOS::ERR::020:: This action isn't allowed to be scheduled. Call this action inline in your own contract and schedule that instead."
    );

}
*/

void croneoscore::sub_balance( const name& owner, const asset& value) {
   deposits_table _deposits( get_self(), owner.value);

   const auto& itr = _deposits.get( value.symbol.code().raw(), "CRONEOS::ERR::021:: No balance with this symbol.");
   check( itr.balance.amount >= value.amount, "CRONEOS::ERR::022:: Overdrawn balance need "+ value.to_string() );

   _deposits.modify( itr, same_payer, [&]( auto& a) {
        a.balance -= value;
    });
}

void croneoscore::add_balance( const name& owner, const asset& value){
   deposits_table _deposits( get_self(), owner.value);
   auto itr = _deposits.find( value.symbol.code().raw() );

   if( itr == _deposits.end() ) {
      //rampayer is get_self(), contract will pay for the deposits table
      _deposits.emplace( get_self(), [&]( auto& a){
        a.balance = value;
      });
   } 
   else {
      _deposits.modify( itr, same_payer, [&]( auto& a) {
        a.balance += value;
      });
   }
}

void croneoscore::sub_reward( const name& miner,  asset value) {
    //change precision of asset
    //*(uint64_t*)&value.symbol = (value.symbol.raw() & ~0xFF) | (value.symbol.raw() & 0xFF) +1;
    uint8_t p = (value.symbol.raw() & 0xFF) +1;
    value.symbol = symbol(value.symbol.code(), p );
    value.amount = value.amount*10;
    rewards_table _rewards( get_self(), miner.value);

    const auto& itr = _rewards.get( value.symbol.code().raw(), "CRONEOS::ERR::021:: No balance with this symbol.");
    check( itr.adj_p_balance.amount >= value.amount, "CRONEOS::ERR::022:: Overdrawn balance");

    _rewards.modify( itr, same_payer, [&]( auto& a) {
        a.adj_p_balance -= value;
    });
}

void croneoscore::add_reward( const name& miner,  asset value, const settings& setting){
    //change precision of asset
    //*(uint64_t*)&value.symbol = (value.symbol.raw() & ~0xFF) | (value.symbol.raw() & 0xFF) +1;
    uint8_t p = (value.symbol.raw() & 0xFF) +1;
    value.symbol = symbol(value.symbol.code(), p );
    int64_t adjusted_amount = value.amount*10;
    value.amount = adjusted_amount*setting.reward_fee_perc/100;

    //pay miner
    rewards_table _rewards( get_self(), miner.value);
    auto itr = _rewards.find( value.symbol.code().raw() );
    if( itr == _rewards.end() ) {
        _rewards.emplace( miner, [&]( auto& a){
            a.adj_p_balance = value;
        });
    } 
    else {
        _rewards.modify( itr, same_payer, [&]( auto& a) {
            a.adj_p_balance += value;
        });
    }
    //pay system
    if(setting.reward_fee_perc == 100){
        //don't pay system
        return;
    }
    asset system_reward = asset(adjusted_amount, value.symbol) - value;
    rewards_table _rewards2( get_self(), get_self().value);
    itr = _rewards2.find( value.symbol.code().raw() );
    if( itr == _rewards2.end() ) {
        _rewards2.emplace( get_self(), [&]( auto& a){
            a.adj_p_balance = system_reward;
        });
    } 
    else {
        _rewards2.modify( itr, same_payer, [&]( auto& a) {
            a.adj_p_balance += system_reward;
        });
    }
}

bool croneoscore::is_valid_fee_token(name tokencontract, asset quantity){

    gastokens_table _gastokens(get_self(), get_self().value);
    auto by_contract_and_symbol = _gastokens.get_index<"bycntrsym"_n>();
    
    uint128_t composite_id = (uint128_t{tokencontract.value} << 64) | quantity.symbol.raw();
    auto token_itr = by_contract_and_symbol.find(composite_id);

    if(token_itr != by_contract_and_symbol.end() ){
        return true;
    }
    else{
        return false;
    }
}

bool croneoscore::is_valid_fee_symbol(symbol& symbol){

    gastokens_table _gastokens(get_self(), get_self().value);
    auto token_itr = _gastokens.find(symbol.raw());

    if(token_itr != _gastokens.end() ){
        return true;
    }
    else{
        return false;
    }
}

name croneoscore::get_contract_for_symbol(symbol sym){
    //croneos will only allow unique SYMBOLS even is from other contract
    gastokens_table _gastokens(get_self(), get_self().value);
    auto row = _gastokens.get(sym.raw());
    return row.token.contract;
}

bool croneoscore::has_scope_write_access(const name& user, const name& scope){
  scopeusers_table _scopeusers(get_self(), scope.value);
  auto itr_scopeusr = _scopeusers.find(user.value);
  if(itr_scopeusr == _scopeusers.end() ){
    return false;
  }
  else{
    return true;
  }

}







