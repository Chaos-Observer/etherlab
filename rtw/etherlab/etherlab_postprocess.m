function etherlab_postprocess(modelName,buildInfo)
% This function does custom post processing after the normal RTW Code 
% Generation is finished, but before the make process
%
% Copyright (c) 2012 Richard Hacker
% License: GPL
%
%

% The following .tlc command parses <model>.rtw to gather parameter
% specific information from the Description field of the blocks,
% such as a Read-Only flag, ENUMerations,
fprintf('### Generating parameter meta information: %s_meta.txt\n', modelName);

% Remove lines with Simulink's stored internal data
perl('postprocess_rtw.pl', [modelName '.rtw'], [modelName '_mod.rtw']);

tlcCmd = ['tlc -r ' modelName '_mod.rtw' ...
        ' ''' which('postprocess.tlc') '''' ...
        ' -aMatlabRoot="' matlabroot '"'];
eval(tlcCmd);
