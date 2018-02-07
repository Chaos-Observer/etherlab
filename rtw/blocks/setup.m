function setup()

if isempty(strfind(path,pwd))
    disp(['Adding ' pwd ' to $MATLABPATH']);
    addpath(pwd);
end

disp(['Precompiling functions in ' pwd]);
mex world_time.c
mex raise.c
mex rtipc_tx.c
mex rtipc_rx.c
mex event.c
mex findidx.c
mex message.c
mex propagate_width.c

if verLessThan('simulink', '9.0')
    system('rm *.slx');
else
    system('rm *.mdl');
end

run EtherCAT/setup.m

return
