function val = subsref(ei,index)
% SUBSREF Define field name indexing for EtherCATInfo objects
val = ei;
for i = 1:length(index)
%    idx = index(i)
    switch index(i).type
        case {'()' '{}'}
            if index(i).subs{1} == ':'
            elseif index(i).subs{1} <= length(val)
                val = val(index(i).subs{1});
            else
                error('Index exceeds matrix dimensions.');
            end
        case '.'
            if isfield(val,index(i).subs)
                val = val.(index(i).subs);
            else
                error('Structure does not have field %s', index(i).subs)
            end
        otherwise
            error('Method %s is not implemented by %s', index(i).type, class(val));
    end
end
