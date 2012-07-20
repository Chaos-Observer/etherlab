function update_devices(block, models)

style = split(get_param(block,'MaskStyleString'),',');
vars =  split(get_param(block,'MaskVariables'),';');

pos = find(strncmp(vars,'model=',6));
if isempty(pos)
    return;
end

modelstr = ['popup(', join(models,'|'), ')'];

if (~strcmp(style{pos}, modelstr))
    style{pos} = modelstr;
    set_param(block, 'MaskStyleString', join(style,','))
    display(['Updated ' block]);
end

function rv = split(s,delim)
rv = regexp(s,regexptranslate('escape',delim),'split');

function rv = join(s,delim)
rv = cell2mat(strcat(reshape(s,1,[]),delim));
rv(end) = '';
