function etherlab_postprocess(modelName,buildInfo)
% This function does custom post processing after the normal RTW Code 
% Generation is finished, but before the make process
%
% Copyright (c) 2006 Richard Hacker
% License: GPL
%
%

% The following .tlc command parses <model>.rtw to gather parameter
% specific information from the Description field of the blocks,
% such as a Read-Only flag, ENUMerations,
tlcCmd = ['tlc -r ' modelName '.rtw' ...
        ' ' which('get_description.tlc') ...
        ' -acFile="' modelName '_meta_data.c"'...
        ' -aMatlabRoot="' matlabroot '"'];
fprintf('### Generating parameter meta information\n%s\n', tlcCmd);
%eval(tlcCmd);
