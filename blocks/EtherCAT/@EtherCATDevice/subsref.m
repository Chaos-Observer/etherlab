function val = subsref(dev,index)
% SUBSREF Define field name indexing for EtherCATDevice objects
val = dev;
for i = 1:length(index)
    switch index(i).type
        case '()'
            [length(val) index(i).subs{1}]
            if length(val) < index(i).subs{1}
                error('Index exceeds matrix dimensions.');
            end
            val = val(index(i).subs{1});
        case '.'
            if strcmpi(index(i).subs,'slaveconfig')
                %% Return a copy of this 
                val = ParseXML;
                fn = fieldnames(dev);
                for i = 1:length(fn)
                    val.(fn{i}) = dev.(fn{i});
                end
            elseif isfield(val,index(i).subs)
                val = val.(index(i).subs);
            else
                error('Structure does not have field %s', index(i).subs)
            end
        otherwise
            error('Method %s is not implemented by %s', index(i).type, class(val));
    end
end
