classdef EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = formatAddress(master,index,tsample)
            rv.master = master(1);

            if numel(tsample) > 1
                rv.domain = tsample(2);
            else
                rv.domain = 0;
            end

            if numel(index) > 1
                rv.alias = index(1);
                rv.position = index(2);
            else
                rv.alias = 0;
                rv.position = index(1);
            end
        end

        %====================================================================
        function scale = configureScale(full_scale,gain,offset,filter)
            %% Return a structure with fields
            %   scale.full_scale = full_scale
            %   scale.gain       = evalin('base',gain)
            %   scale.offset     = evalin('base',offset)
            %   scale.filter      = evalin('base',filter)
            % if gain, offset and filter are not all ''
            % otherwise, return false

            % Make sure all options are set
            if nargin < 3
                offset = [];
            end
            if nargin < 4
                filter = [];
            end

            % If everything is empty, no scaling and return
            if isempty([gain,offset,filter])
                scale = false;
                return
            end

            % Make sure every parameter is set
            scale.gain = gain;
            scale.offset = offset;
            scale.filter = filter;

            if isempty(scale.gain) && isempty(scale.offset)
                scale.full_scale = 1;
            else
                scale.full_scale = full_scale;
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static, Access = protected)
        %====================================================================
        % This function returns an array that will address every
        % Pdo Entry that is non-zero and have the correct direction
        % given the SyncManager list
        % The return value is a Nx7 array, where columns
        %       1:4   = [SmIdx,PdoIdx,PdoEntryIdx,0] address
        %       5:6   = [PdoEntryIndex,PdoEntrySyubIndex,BitLen]
        function entry = findPdoEntries(Sm, dir)
            if isempty(Sm)
                entry = [];
                return
            end

            % Find the syncmanagers with the correct directions
            % The result is a cell array where each cell consists of:
            %   SmIdx, -- Syncmanager Index
            %   Pdo,   -- Cell array of pdo's
            sm = arrayfun(@(smIdx) {smIdx-1,Sm{smIdx}{3}}, ...
                           find(cellfun(@(x) x{2} == dir, Sm)), ...
                           'UniformOutput', false);

            if isempty(sm)
                entry = [];
                return
            end

            % Split up the individual syncmanagers into pdo's
            % The result is a vertical cell array where each cell has
            %   SmIdx, -- Syncmanager Index
            %   PdoIdx, -- Index of pdo
            %   PdoEntry, -- Nx4 Array of Pdo Entries
            pdo = cellfun(...
                @(pdo) mat2cell(horzcat(repmat({pdo{1}},...
                                               numel(pdo{2}),1),...
                                        num2cell(0:numel(pdo{2})-1)',...
                                        reshape(pdo{2},[],1)),...
                                ones(1,numel(pdo{2}))),...
                sm, 'UniformOutput', false);
            pdo = vertcat(pdo{:});

            % Split up the individual pdo's into pdo entries
            % The result is an array where each row consists of
            %   SmIdx, -- Syncmanager Index
            %   PdoIdx, -- Index of pdo
            %   ElementIdx, -- (=0), Index of the element
            %   PdoEntryIdx, -- Index of Pdo Entry
            %   EtherCAT PdoEntryIndex
            %   EtherCAT PdoEntrySubIndex
            %   EtherCAT BitLen
            entry = cellfun(...
                @(e) horzcat(repmat([e{[1,2]},0],size(e{3}{2},1),1),...
                             (0:size(e{3}{2},1)-1)',...
                             e{3}{2}),...
                pdo, 'UniformOutput', false);
            entry = vertcat(entry{:});

            % - Remove rows where PdoEntryIndex == 0
            % - Swap columns 4 and 3
            if ~isempty(entry)
                entry = entry(entry(:,5) ~= 0,[1,2,4,3,5:end]);
            end
        end
        
        %====================================================================
        % Return the row where the name in the first column matches
        function slave = findSlave(name,models)
            row = find(strcmp(models(:,1), name));

            if isempty(row)
                slave = [];
            else
                slave = models(row,:);
            end
        end

        %====================================================================
        function port = configurePorts(name,pdo,dtype,vector,scale)

            if nargin < 5
                scale = false;
            end

            pdo_count = size(pdo,1);

            if ~pdo_count
                port = [];
                return
            end

            if vector
                port.pdo = pdo(:,1:4);
                port.pdo_data_type = dtype;
                port.portname = name;

                if isa(scale,'struct')
                    port.full_scale = scale.full_scale;
                    port.gain   = {'Gain',  scale.gain};
                    port.offset = {'Offset',scale.offset};
                    port.filter = {'Filter',scale.filter};
                elseif scale
                    port.full_scale = [];
                    port.gain   = [];
                    port.offset = [];
                    port.filter = [];
                end
            else
                port = arrayfun(...
                        @(i) struct('pdo',pdo(i,1:4),...
                                    'pdo_data_type',dtype,...
                                    'portname', [name,int2str(i)]), ...
                        1:pdo_count);

                if isa(scale,'struct') || scale
                    port(1).full_scale = [];
                    port(1).gain   = [];
                    port(1).offset = [];
                    port(1).filter = [];
                end

                if ~isa(scale,'struct')
                    return
                end

                % Replicate gain, offset and filter if there is only on element
                if numel(scale.gain) == 1
                    scale.gain = repmat(scale.gain,1,pdo_count);
                end
                if numel(scale.offset) == 1
                    scale.offset = repmat(scale.offset,1,pdo_count);
                end
                if numel(scale.filter) == 1
                    scale.filter = repmat(scale.filter,1,pdo_count);
                end

                for i = 1:pdo_count
                    idxstr = int2str(i);

                    port(i).full_scale = scale.full_scale;

                    if i <= numel(scale.gain)
                        port(i).gain   = {['Gain', idxstr], scale.gain(i)};
                    end

                    if i <= numel(scale.offset)
                        port(i).offset = {['Offset', idxstr], scale.offset(i)};
                    end

                    if i <= numel(scale.filter)
                        port(i).filter = {['Filter', idxstr], scale.filter(i)};
                    end
                end
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Abstract)
        models
    end
end
