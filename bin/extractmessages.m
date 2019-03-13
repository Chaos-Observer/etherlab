%----------------------------------------------------------------------------
%
% $Id$
%
%----------------------------------------------------------------------------
messageblocks = find_system(bdroot,...
    'LookUnderMasks','all',...
    'ReferenceBlock','etherlab_lib/Util/Message');
types = get_param(messageblocks, 'prio');

feval(bdroot,[],[],[],'compile') 

fd = fopen(strcat('messages-',bdroot,'.txt'), 'w');

start = numel(bdroot)+1;
for i = 1 : size(messageblocks,1)
    % RTW replaces newlines with spaces

    ph = get_param(messageblocks{i},'PortHandles'); 
    len = get_param(ph.Outport, 'CompiledPortWidth');
    if isempty(get_param(messageblocks{i},'message_text'))
        disp([messageblocks{i} ' has no message'])
    end
    
    % Strip model name from path
    fprintf(fd, '%s\t%s\t%i\n', messageblocks{i}(start:end), types{i}, len);
end
fclose(fd);

feval(bdroot,[],[],[],'term') 

%----------------------------------------------------------------------------
