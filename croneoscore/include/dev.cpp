ACTION croneoscore::clear() { //temp action
    require_auth(get_self());
    cleanTable<cronjobs_table>(get_self(), get_self().value, 20);
}

ACTION croneoscore::delrewards(name scope){
    require_auth(get_self());
    cleanTable<rewards_table>(get_self(), scope.value, 20);
}

ACTION croneoscore::delsettings(){
  require_auth(get_self());
  settings_table _settings(get_self(), get_self().value);
  //auto settings = _settings.get();
  _settings.remove();

}

ACTION croneoscore::test(eosio::name pair, eosio::extended_asset token){
  require_auth(get_self());
  bancor::get_eos_value_of(pair, token);

}