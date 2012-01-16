function setup()

if ~length(strfind(path,pwd))
    disp(['Adding ' pwd ' to $MATLABPATH']);
    addpath(pwd);
end

disp(['Precompiling functions in ' pwd]);
mex world_time.c
mex raise.c
mex etherlab_in.c
mex etherlab_out.c
mex event.c
mex findidx.c

run EtherCAT/setup.m

return
