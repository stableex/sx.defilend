# **`SX.DefiLend`**

> C++ intetrface for interacting with `lend.defi` smart contract


## Quickstart

```c++
#include <sx.defilend/defilend.hpp>

const asset in = asset { 100000, "USDT" };
const symbol out_sym = symbol { "BUSDT,4" };

// calculate out price
const asset out = defilend::get_amount_out( in, out_sym);
// => "8.6500 BUSDT"
```

```c++
const asset in = asset { 100000, "USDT" };

// calculate wrapped amount
const asset btokens = defilend::wrap( in );
// => "8.6500 BUSDT"
```

```c++
const asset in = asset { 100000, "BUSDT" };

// calculate unwrapped amount
const asset tokens = defilend::unwrap( in );
// => "11.4500 USDT"
```
