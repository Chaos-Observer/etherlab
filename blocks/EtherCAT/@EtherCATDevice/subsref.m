function val = subsref(dev,index)
% SUBSREF Define field name indexing for EtherCATDevice objects
val = dev;
for i = 1:length(index)
    switch index(i).type
        case '()'
            if length(val) < index(i).subs{1}
                error('Index exceeds matrix dimensions.');
            end
            val = val(index(i).subs{1});
        case '.'
            if ~isfield(val,index(i).subs)
                error('Structure does not have field %s', index(i).subs)
            end
            val = val.(index(i).subs);
        otherwise
            error('Method %s is not implemented by %s', index(i).type, class(val));
    end
end
