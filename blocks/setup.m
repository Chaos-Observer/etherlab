function setup()

disp(['Precompiling functions in ' pwd]);
mex taskinfo.c
mex world_time.c
mex -I/opt/etherlab/rtw-3.4/include raise.c
return
