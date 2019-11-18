<h1 class="contract">exec</h1>
---
spec_version: "0.2.0"
title: Execute
summary: 'Execute scheduled job.'
icon: https://croneos.io/statics/app-logo-128x128.png#d39fc7767099890429d926339b4d13af66b77db9b443366002227bad9bb1896e
---
{{executer}} agrees to execute cronjob with id {{id}} and acknowledge that CPU will be used.

<h1 class="contract">schedule</h1>
---
spec_version: "0.2.0"
title: Schedule Job
summary: 'Schedule a new cronjob.'
icon: https://croneos.io/statics/app-logo-128x128.png#d39fc7767099890429d926339b4d13af66b77db9b443366002227bad9bb1896e
---
{{owner}} agrees to schedule a new cronjob and acknowledge that this action will temporary consume RAM until the job is executed or cancelled.

<h1 class="contract">cancel</h1>
---
spec_version: "0.2.0"
title: Cancel
summary: 'Cancel scheduled job.'
icon: https://croneos.io/statics/app-logo-128x128.png#d39fc7767099890429d926339b4d13af66b77db9b443366002227bad9bb1896e
---
{{owner}} agrees to cancel cronjob with id {{id}} and will release RAM used to store the job.

<h1 class="contract">refund</h1>
---
spec_version: "0.2.0"
title: Refund Deposit
summary: 'Refund deposit.'
icon: https://croneos.io/statics/app-logo-128x128.png#d39fc7767099890429d926339b4d13af66b77db9b443366002227bad9bb1896e
---
{{owner}} agrees to receive a refund of {{amount}} via an inline transfer initiated by the contract. The {{asset_to_symbol_code amount}} will be substracted from {{owner}}'s deposit.

<h1 class="contract">withdraw</h1>
---
spec_version: "0.2.0"
title: Withdraw reward balance
summary: 'Withdraw miner rewards.'
icon: https://croneos.io/statics/app-logo-128x128.png#d39fc7767099890429d926339b4d13af66b77db9b443366002227bad9bb1896e
---
{{miner}} agrees to receive {{amount}} via an inline transfer initiated by the contract. The {{asset_to_symbol_code amount}} will be substracted from {{miner}}'s miner rewards.
