function setup_etherlab()

if ~length(strfind(path,pwd))
    disp(['Adding ' pwd ' to $MATLABPATH']);
end
addpath(pwd);

run blocks/setup.m

try
    savepath;
catch
    disp(['ERROR: Could not save MATLABPATH. Probably ' ...
	'you cannot write the file. Check that ' ...
	'you have write permissions for '...
	'$MATLABPATH/toolbox/local/pathdef.m']);
end


return
