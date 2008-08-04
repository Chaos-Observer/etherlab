function setup()

if ~length(strfind(path,pwd))
    disp(['Adding ' pwd ' to $MATLABPATH']);
    addpath(pwd);
end

disp(['Precompiling functions in ' pwd]);
mex cif_pd_in.c
mex cif_pd_out.c
return
