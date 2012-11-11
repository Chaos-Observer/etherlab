function update_mask_state(class, names, enables)
%% This function sets the mask parameters to be visible or not

masknames  = get_param(gcbh,'MaskNames');
maskstatus = get_param(gcbh,class);

% Find out which indices are valid
if ~iscell(names)
    names = {names};
end
idx = cellfun(@(x) find(strcmp(masknames,x), 1), names, 'UniformOutput', false);
notempty = cellfun(@(x) ~isempty(x), idx);

if numel(enables) == 1
    enables = repmat(enables, 1, numel(names));
end

on = logical(enables(notempty));
idx = cell2mat(idx(notempty));

maskstatus(idx(on))  = repmat({'on'},  sum(on),  1);
maskstatus(idx(~on)) = repmat({'off'}, sum(~on), 1);

set_param(gcbh, class, maskstatus);
%maskstatus
