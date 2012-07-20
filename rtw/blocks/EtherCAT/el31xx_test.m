function el31xx_test

load 'el31xx_cases'

rv = el31xx('EL3164', 'Vector Output', 0, 'Double', 1, 0, 0, 0);
rv_stored = el31xx_case1;
assert(isequal(rv_stored.SlaveConfig.product, rv.SlaveConfig.product))
assert(isequal(rv_stored.SlaveConfig.sm, rv.SlaveConfig.sm))

rv = el31xx('EL3102', 'Vector Output', 0, 'Double', 1, 0, 0, 0);
rv_stored = el31xx_case2;
assert(isequal(rv_stored.SlaveConfig.product, rv.SlaveConfig.product))
assert(isequal(rv_stored.SlaveConfig.sm, rv.SlaveConfig.sm))

rv = el31xx('EL3102', 'NO Vector Output', 1, 'Double', 1, 0, 0, 0);
rv_stored = el31xx_case3;
assert(isequal(rv_stored.SlaveConfig.product, rv.SlaveConfig.product))
assert(isequal(rv_stored.SlaveConfig.sm, rv.SlaveConfig.sm))

