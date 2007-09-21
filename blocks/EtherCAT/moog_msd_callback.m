%-----------------------------------------------------------------------------
%
% $Id$
%
%-----------------------------------------------------------------------------

function moog_msd_callback(action,block)

feval(action,block)

%-----------------------------------------------------------------------------

function display_callback(block)

choices = {'Inputs and Outputs', 'Inputs only', 'Outputs only'};
iomode = find(strcmp(get_param(block, 'iomode'), choices));

title = 'fprintf(''Moog Servo Drive\nM:%i Idx:%s'', master, index)';
inputPortStr = 'port_label(''input'', 1, ''control'') port_label(''input'', 2, ''v_tgt'')';
outputPortStr = 'port_label(''output'', 1, ''status'') port_label(''output'', 2, ''v_cur'')';

if (iomode == 1)
    dsp = [title inputPortStr outputPortStr];
elseif (iomode == 2)
    dsp = [title inputPortStr];
elseif (iomode == 3)
    dsp = [title outputPortStr];
else
    dsp = strcat('dsp(''ERROR'')');
end

set_param(block, 'MaskDisplay', dsp);

%-----------------------------------------------------------------------------
