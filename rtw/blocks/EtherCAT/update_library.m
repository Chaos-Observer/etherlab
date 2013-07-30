function update_library
% Goes through every block in the library and calls function gcb.UserData.Update
% as F('Update')

s = find_system(gcs);
for i = 1:numel(s)
    %display(s{i})
    if ~strcmp(s{i},gcs)
        ud = get_param(s{i},'UserData');
        if isfield(ud,'Update') && isa(ud.Update, 'function_handle')
            feval(ud.Update,'Update',s{i});
        end
    end
end
