function setup()

if ~length(strfind(path,pwd))
    disp(['Adding ' pwd ' to $MATLABPATH']);
    addpath(pwd);
end

disp(['Precompiling functions in ' pwd]);
mex xPCI1710_inc.c
mex xPCI1710_etm.c
return
