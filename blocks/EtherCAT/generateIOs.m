function [PdoEntryMap EtherCATInfo_slave] = generateIOs(varargin)
    % Select:
    %   - Empty:
    %       selects all PDO Entries where there are no conflicts
    %
    %   - Vector or array with 1 column: 
    %       selects all PDO Entries of a PDO with matching PDO Index
    %
    %   - Array with 2 columns: select PDO Entries where the 
    %       * first column specifies the PDO Entry Index and
    %       * second column specifies the PDO Entry SubIndex
    %         If zero, select all with the PDO Entry Index
    %       Example: (? = ignored)
    %       [[ x 0 ]   selects all Pdo Entries where PDO Entry Index == x
    %        [ x y ]]  selects all Pdo Entries where PDO Entry Index == x
    %                   and PDO Entry SubIndex = y
    %

    PdoEntryMap = [];
    EtherCATInfo_slave = [];

    % See the documentation for the interpretation of every
    % field.
    arg_list = struct(...
        'SelectPdo', {[]...
            {'Vector' ...
            ['\tIf specified, all Pdo Entries of PDO Indices contained\n'...
             '\tin this list are selected']}
             }, ...
        'SelectPdoEntry',{ [] ...
            {'M-by-2 array' 
            '\n'} ...
            },...
        'ProductCode',{ [], ...
            {'Scalar' 
            '\n'} ...
            },...
        'RevisionNo',{[], ...
            {'Scalar'
            '\n'} ...
            },...
        'GroupType',{[], ...
            {'String: ''None'' ''Pdo'' ''All'''
            '\n'} ...
            },...
        'SmAssignment', { zeros(0,2), ...
            {'M-my-2 Array'
            '\n'} ...
            },...
        'Debug',{ 0, ...
            {'Scalar' 
            '\n'} ...
            });
        
    global arg
    arg = arg_list(1);
    
    argc = length(varargin);
    
    if ~argc
        help generateIOs
        fprintf('Available options: \n');
        fn = fieldnames(arg);
        for i = 1:length(fn)
            fprintf('    %s:\t%s\n', fn{i}, arg_list(2).(fn{i}){1});
            fprintf(arg_list(2).(fn{i}){2});
        end
        return
    end
    
    EtherCATInfo = varargin{1};
    
    % Generate a map where the lowercase field names of arg
    % contain the real field names,i.e.
    % f.revisionno = 'RevisionNo', etc...
    fn = fieldnames(arg);
    fn_lower = lower(fn);
    f = cell2struct(fn, fn_lower, 1);
    
    for i = 1:(argc-1)/2
        field = varargin{i*2};
        switch lower(field)
            case fn_lower
                arg.(f.(lower(field))) = varargin{i*2+1};
            otherwise
                fprintf('Ignoring unknown parameter ''%s''', field);
        end
    end

    if ischar(arg.GroupType)
        arg.GroupType = find(strcmpi({'None' 'Pdo' 'All'}, ...
            arg.GroupType));
        arg.GroupType = arg.GroupType - 1;
        if arg.Debug && isempty(arg.GroupType)
            fprintf('Specified value for argument ''GroupType'' is unknown\n');
        end
    elseif ~isnumeric(arg.GroupType)
        arg.GroupType = [];
    end

    if isempty(arg.GroupType)
        arg.GroupType = 0;
    end

    if isstruct(EtherCATInfo)
        EtherCATInfo = findDevice(EtherCATInfo, arg.ProductCode, arg.RevisionNo);
    else
        try
            EtherCATInfo = getEtherCATInfo(EtherCATInfo, ...
                'ProductCode', arg.ProductCode, 'RevisionNo', arg.RevisionNo);
        catch
            if arg.Debug
                fprintf('An error occurred while trying to load %s: %s\n', ...
                    char(EtherCATInfo), lasterr)
            end
        end
    end
    
    if isempty(EtherCATInfo.Device)
        return
    end

    arg.SelectAll = 0;
    arg.SelectPdoIdx = [];
    arg.SelectPdoEntryIdx = [];
    arg.SelectExact = [];
    if isempty(arg.SelectPdo) && isempty(arg.SelectPdoEntry)
        % 1) wildcard: 
        %       Select everything when Pdo Index and Pdo Entry Index is zero
        arg.SelectAll = 1;
    elseif isnumeric(arg.SelectPdo) && ~isempty(arg.SelectPdo)
        % 2) Pdo Entry Index wildcard
        %       Select all Pdo Entries in the contained in Pdo Index
        % arg.SelectPdo is a 1-by-N vector
        arg.SelectPdoIdx = reshape(arg.SelectPdo,1,numel(arg.SelectPdo));
    elseif isnumeric(arg.SelectPdoEntry) && ~isempty(arg.SelectPdoEntry)
        % 3) Pdo Entry SubIndex wildcard:
        %       Select all Pdo Entries with the matching Index
        % arg.SelectPdoEntry is a 1-by-N vector
        if size(arg.SelectPdoEntry,2) == 1
            arg.SelectPdoEntryIdx = ...
                reshape(arg.SelectPdoEntry,1,numel(arg.SelectPdoEntry));
        else
            c = and( arg.SelectPdoEntry(:,1), ~arg.SelectPdoEntry(:,2));
            arg.SelectPdoEntryIdx = arg.SelectPdoEntry(c,1)';
            
            % 4) Exact match:
            %       Select all Pdo Entries with the matching Index
            %       and SubIndex
            % arg.SelectPdoEntry is a M-by-2 matrix
            %     [ PdoEntryIndex PdoEntrySubIndex; ... 
            %       ...           ...              ]
            r = and(arg.SelectPdoEntry(:,1), arg.SelectPdoEntry(:,2));
            arg.SelectExact = arg.SelectPdoEntry(r,:);
        end

    else
        fprintf('Invalid ''Select'' argument value\n');
        return
    end
                    
    if arg.Debug
        if arg.SelectAll
            fprintf('All Pdo Entries are selected\n');
        else
            fprintf('Summary of Pdo Entry selection: select where\n');
            if ~isempty(arg.SelectPdoIdx)
                fprintf('\t* Pdo Index is %s\n', ...
                    mat2str(arg.SelectPdoIdx));
            end
            if ~isempty(arg.SelectPdoEntryIdx)
                fprintf('\t* Pdo Entry Index is %s\n', ...
                    mat2str(arg.SelectPdoEntryIdx));
            end
            if ~isempty(arg.SelectExact)
                fprintf('\t* where [PdoEntryIndex, PdoEntrySubIndex] is %s\n', ...
                    mat2str(arg.SelectExact));
            end
        end
    end
           
    % Make sure that the SyncManager Indices are unique
    overlapping = intersect(EtherCATInfo.Device(1).OutputSm, ...
        EtherCATInfo.Device(1).InputSm);
    if ~isempty(overlapping)
        error('generateIOs:generateIOs:EtherCATInfoError',...
            ['SyncManager Index %s exist in both input and output '...
            'SyncManagers; Exiting...\n'], mat2str(overlapping));
        return
    end

    if size(arg.SmAssignment,2) ~= 2 || ~isnumeric(arg.SmAssignment)
        error('generateIOs:generateIOs:ParameterError',...
            ['Value for argument ''SmAssignment'' must be an M-by-2 ', ...
            'numeric matrix\n'])
    elseif length(unique(arg.SmAssignment(:,1))) ~= size(arg.SmAssignment,1)
        error('generateIOs:generateIOs:EtherCATInfoError',...
            ['Some Pdo Indices in parameter ''SmAssignment'' are ' ...
            'specified multiply.']);
        return
    end

    EtherCATInfo_slave = struct(...
        'VendorId', EtherCATInfo.VendorId,...
        'Type', EtherCATInfo.Device(1).Type,...
        'ProductCode', EtherCATInfo.Device(1).ProductCode,...
        'RevisionNo', EtherCATInfo.Device(1).RevisionNo,...
        'Name', EtherCATInfo.Device(1).Name);

    [TxPdoEntryMap TxSm] = getPdoEntryGroups(...
        EtherCATInfo.Device(1).TxPdo, ...
        EtherCATInfo.Device(1).InputSm, 1, ...
        arg.GroupType);
    
    [RxPdoEntryMap RxSm] = getPdoEntryGroups(...
        EtherCATInfo.Device(1).RxPdo, ...
        EtherCATInfo.Device(1).OutputSm, 0, ...
        arg.GroupType);
    
    EtherCATInfo_slave.SyncManager = {TxSm{2}{:} RxSm{2}{:}};
    
    PdoEntryMap = {TxPdoEntryMap{:} RxPdoEntryMap{:}};
end

function [groups SmPdo] = getPdoEntryGroups(PdoList, SmList, SmDir, GroupType)
    % Returns a cell vector of M-by-4 arrays of all pdo entries 
    % listed in the Pdo structure array passed as an argument
    % i.e {  M1-by-4, M2-by-4, ... }
    % where the first cell is a list of all Pdo Entries grouped 
    % according to arg.

    SmPdo = {SmList {}};
    for i = 1:length(SmList)
        SmPdo{2}{i} = struct('SmIndex', SmList(i), 'SmDir', SmDir, ...
            'Pdo', repmat(PdoList,1,0));
    end
    
    % Not all Pdos are allowed to be mapped. A mapped Pdo can exclude
    % other pdos. The exclude variable below contains the excludes of all
    % selected pdo's
    exclude = [];

    entries = [];
    for i = 1:length(PdoList)
        [newentries SmPdo exclude] = ...
            getPdoEntries(PdoList(i), SmPdo, exclude);
        entries = [entries; newentries];
    end
    
    groups = groupEntries(entries, GroupType);
end

function [entries SmPdo exclude] = getPdoEntries(Pdo, SmPdo, exclude)
    % Returns a M-by-4 array of all Pdo Entries contained in a Pdo
    % [ PdoIndex PdoEntryIndex PdoEntrySubIndex PdoEntryDataType; ...]
    
    global arg
    
    % First work horizontally with entries - Matlab extends colums
    % faster than rows. entries has 4 rows:
    % 1: Pdo Index
    % 2: Pdo Entry Index
    % 3: Pdo Entry SubIndex
    % 4: Pdo Entry DataType
    entries = repmat(zeros(4,1),1,0);
    

    for i = 1:length(Pdo)
        % Generate an 3-by-N array of all pdo entries existing in this Pdo.
        % The rows have the meaning: Index, SubIndex, DataType
        %  [ Index    ...;
        %    SubIndex ...;
        %    DataType ...]
        %
        % el = element list
        newentries = repmat(zeros(4,1),1,max(length(Pdo.Entry),1));
        PdoEntryCount = 1;
        for j = 1:length(Pdo.Entry)
            PdoEntry = Pdo(i).Entry(j);
            
            % Ignore spacer Pdo Entries where Index == 0
            if PdoEntry.Index
                newentries(:,PdoEntryCount) = ...
                    [Pdo(i).Index PdoEntry.Index PdoEntry.SubIndex PdoEntry.DataType]';
                PdoEntryCount = PdoEntryCount + 1;
            end
        end
        
        if Pdo.Sm < 0
            candidate = arg.SmAssignment(arg.SmAssignment(:,1) == Pdo.Index, 2);
            if length(candidate)
                if isempty(intersect(SmPdo{1}, candidate(1))) 
                    error('generateIOs:getPdoEnties:nonExistent',...
                        ['The SyncManager specified for Pdo Index #x%04X '...
                        'using the ''SmAssignment'' argument does not exist ' ...
                        'Available are: %s'], Pdo.Index, mat2str(SmPdo{1}));
                end
                Pdo.Sm = candidate(1);
            else
                if length(SmPdo{1}) == 1
                    Pdo.Sm = SmPdo{1}(1);
                else
                    if arg.Debug
                        fprintf(['Warning: Pdo #x%04x does not have a default '...
                            'SyncManager and there are more than 1 to choose '...
                            'from. Set the SyncManager using the ''SmAssignment'' ' ...
                            'option. Skipping...\n'], Pdo.Index)
                    end
                    continue
                end
            end
        end


        idx = find(SmPdo{1} == Pdo.Sm);
        if isempty(idx)
            if arg.Debug
                fprintf(['Could not find SyncManager Index %u; '...
                    'available are %s; skipping ...\n'], Pdo.Sm, ...
                mat2str(SmPdo{1}));
            end
            continue
        end
        
        j = length(SmPdo{2}{idx}.Pdo) + 1;
        SmPdo{2}{idx}.Pdo(j) = Pdo;

        % Delete remaining columns that were skipped due to spacer pdo
        % entries
        newentries(:, PdoEntryCount:length(Pdo.Entry)) = [];
        
        if arg.SelectAll || Pdo.Mandatory
            cols = 1:PdoEntryCount-1;
        else
%             fprintf('Checking Pdo Idx %s %s\n', ...
%                 mat2str(newentries(  1,:)), mat2str(arg.SelectPdoIdx));
%             fprintf('Checking Pdo Entry Idx %s %s\n', ...
%                 mat2str(newentries(  2,:)), mat2str(arg.SelectPdoEntryIdx));
%             fprintf('Checking Pdo Entry SubIdx %s %s\n', ...
%                 mat2str(newentries(2:3,:)), mat2str(arg.SelectExact));
            cols = or(or(ismember(newentries(  1,:),  arg.SelectPdoIdx), ...
                         ismember(newentries(  2,:),  arg.SelectPdoEntryIdx)), ...
                         ismember(newentries(2:3,:)', arg.SelectExact, 'rows')');
        end

        if ~isempty(intersect(exclude, Pdo(i).Index))
            if arg.Debug
                fprintf(['Pdo Index #x%04X was found in exclusion list '...
                    'of Pdos that are selected already; skipping...\n'], ...
                    Pdo(i).Index);
            end
            continue
        end
        
        entries = [entries newentries(:,cols)];
        if isfield(Pdo(i),'Exclude')
            if arg.Debug
                fprintf('Putting %s in exclusion list\n', ...
                    cell2mat(cellfun(@(x) sprintf(' #x%04X',x), ...
                    {Pdo(i).Exclude}, 'uniformoutput',0)));
            end
            exclude = [exclude Pdo(i).Exclude];
        end
    end
    
    % Now flip the array into the M-by-4 version
    entries = entries';
end

function PdoEntryMap = groupEntries(entries,type)
    rows = size(entries,1);
    if ~rows
        PdoEntryMap = {};
        return
    end
    switch type
        case 0
            % 1) Slice out row 2 and 3 of entries and transpose ==>
            %    entries([2 3],:)'. This yields a M-by-2 matrix where the
            %    [PdoEntryIndex PdoEntrySubIndex; ...]
            % 2) Split up all rows of the above matrix into M cells, each
            %    having one vector [PdoEntryIndex PdoEntrySubIndex]
            PdoEntryMap = mat2cell(entries(:,[2 3]),ones(rows,1), 2);
            
        case 1
            % Group all Pdo Entries that have the same PdoIndex and
            % data type
            PdoGroup = group(entries, 1, 0);
            
            PdoEntryMap = {};
            idx = 1;
            for i = 1:length(PdoGroup)
                DataTypeGroup = group(PdoGroup{i},4, 1);
                for j = 1:length(DataTypeGroup);
                    PdoEntryMap{idx} = DataTypeGroup{j};
                    idx = idx+1;
                end
            end
            
        case 2
            % Group all Pdo Entries that have the same data type
            PdoEntryMap = group(entries, 4, 1);
    end
end

function y = group(entries, column, last)
    % This function returns a cell array of M-by-4 (or M-by-2 if last
    % is set) arrays grouped along the column
    rows = size(entries,1);
    list = entries(:,column);
    uniq = unique(list)';
    group_count = length(uniq);
    select = repmat(list,1,group_count) == repmat(uniq, rows, 1);
    y = cell(1,group_count);
    
    if last
        % If last is set, only select columns 2 (Pdo Entry Index)
        % and 3 (Pdo Entry SubIndex)
        cols = [2 3];
    else
        % Return all columns
        cols = 1:4;
    end
    
    for i = 1:group_count
        y{i} = entries(select(:,i),cols);
    end
end

