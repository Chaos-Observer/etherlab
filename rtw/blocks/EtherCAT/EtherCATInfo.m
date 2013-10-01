classdef EtherCATInfo < XmlNode
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        function rv = hexDecValue(s)
            if numel(s) > 2 && strcmp(s(1:2),'#x')
                rv = hex2dec(s(3:end));
            else
                rv = str2double(s);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods
        %------------------------------------------------------------------
        function ei = EtherCATInfo(f)
            doc = xmlread(f);
            ei = ei@XmlNode(doc.getDocumentElement);
            ei.doc = doc;
        end

        %------------------------------------------------------------------
        function list = getSlaves(obj)
            descriptions = obj.getFirstNode('Descriptions');
            devices = descriptions.getFirstNode('Devices');
            slaves = devices.getNodes('Device');
            list = cellfun(@(x) EtherCATInfoSlave(x), slaves, ...
                                'UniformOutput', false);
        end

        %------------------------------------------------------------------
        function slaves = getSlave(obj,code,class,revision)
            %% Return a list of slaves
            % arguments:
            %   code: string:name | numeric:ProductCode
            %   class: 'class'|'revision'
            %   revision: number for class
            descriptions = obj.getFirstNode('Descriptions');
            devices = descriptions.getFirstNode('Devices');
            slaves = cellfun(@(x) EtherCATInfoSlave(x), ...
                             devices.getNodes('Device'), ...
                             'UniformOutput', false);

            if nargin == 1
                return
            elseif isstr(code)
                slaves = slaves(cellfun(@(s) strcmp(s.Name,code), slaves));
            elseif isnumeric(code)
                slaves = slaves(cellfun(@(s) s.ProductCode == code, slaves));
            else
                return
            end

            if nargin > 3
                switch class
                case 'class'
                    slaves = slaves(cellfun(@(s) (s.RevisionNumber & 65535) == revision, ...
                                            slaves));
                case 'revision'
                    slaves = slaves(cellfun(@(s) s.RevisionNumber == revision, ...
                                            slaves));
                end
            end

            hidden = unique(cell2mat(horzcat(...
                cellfun(@(s) s.hideTypes,slaves,'UniformOutput', false))));
            rev = cellfun(@(x) x.RevisionNumber, slaves);

            slaves = slaves(~ismember(rev,hidden));
        end

        %------------------------------------------------------------------
        function testConfiguration(obj, SlaveConfig, PortConfig)
            if ~isempty(SlaveConfig)
                obj.testSlaveConfig(SlaveConfig)
            end
        end

        %------------------------------------------------------------------
        function testSlaveConfig(obj,SlaveConfig)
            % Make sure .vendor exists and is correct
            if ~isfield(SlaveConfig,'vendor')
                disp('Vendor missing');
            elseif str2double(obj.getFirstNode('Vendor').getFirstNode('Id').getTextContent) ~= SlaveConfig.vendor
                disp('Vendor incorrect');
            end

            % Make sure .product exists and is correct
            if ~isfield(SlaveConfig,'product')
                disp('ProductCode missing');
            else
                slave = obj.getSlave(SlaveConfig.product);
                if isempty(slave)
                    fprintf('Slave with ProductCode=#x%x does not exist\n', ...
                            SlaveConfig.product);
                    return
                end

                if numel(slave) > 1 && isfield(SlaveConfig,'description')
                    names = cellfun(@(x) x.Name, slave, 'UniformOutput', false);
                    slave = slave(strcmp(names,SlaveConfig.description));

                    if isempty(slave)
                        fprintf('Slave %s does not exist\n', ...
                                SlaveConfig.description);
                        return
                    end
                end

                slave = slave{1};
            end

            % Compare description
            if isfield(SlaveConfig,'description')
                if ~strcmp(slave.Name, SlaveConfig.description)
                    fprintf('Slave Description for #x%x do not match\n',...
                        SlaveConfig.product);
                end
            end

            % Finish here if there is no .sm
            if ~isfield(SlaveConfig,'sm') || ~numel(SlaveConfig.sm)
                return
            end

            if isfield(SlaveConfig,'dc') && ~isempty(SlaveConfig.dc) ...
                        && SlaveConfig.dc(1) ~= 0
                if numel(SlaveConfig.dc) ~= 10
                    fprintf('DC has %i elements instead of 10\n', ...
                        numel(SlaveConfig.dc));
                end

                assignActivate = cellfun(@(x) EtherCATInfo.hexDecValue(x.getFirstNode('AssignActivate').getTextContent), ...
                    slave.getFirstNode('Dc').getNodes('OpMode'));
                if ~any(ismember(SlaveConfig.dc(1), assignActivate))
                    fprintf('DC AssignActivate=#x%x is not found\n', ...
                        SlaveConfig.dc(1));
                end
            end

            sm = slave.getNodes('Sm');
            for i = 1:numel(SlaveConfig.sm)
                % .sm must have 3 elements
                if numel(SlaveConfig.sm{i}) < 3
                    fprintf('SlaveConfig.sm{%i} does not have 3 elements\n',i);
                    return
                end
                smIdx = SlaveConfig.sm{i}{1}+1;

                % Test whether there are enough <Sm>'s
                if smIdx > numel(sm)
                    fprintf('SmIdx SlaveConfig.sm{%i}{1}=%i does not exist\n',...
                        i, SlaveConfig.sm{i}{1});
                    return
                end

                dirText = sm{smIdx}.getTextContent;
                control_byte = EtherCATInfo.hexDecValue(sm{smIdx}.getAttribute('ControlByte'));

                % Check Sm direction
                % {2} == 0 is for RxPdo's
                % {2} == 1 is for TxPdo's
                if (SlaveConfig.sm{i}{2} ...
                        && (~(isempty(dirText) || strcmp(dirText,'Inputs')) ...
                             || bitand(control_byte,4)) ...
                   || (~SlaveConfig.sm{i}{2} ...
                        && (~(isempty(dirText) || strcmp(dirText,'Outputs')) ...
                             || ~bitand(control_byte,4))))
                    fprintf('Sm direction of SlaveConfig.sm{i}{2}=%i is incorrect\n', ...
                        i, SlaveConfig.sm{i}{2});
                end

                if SlaveConfig.sm{i}{2}
                    pdo_dir = 'TxPdo';
                else
                    pdo_dir = 'RxPdo';
                end
                pdo = slave.getNodes(pdo_dir);
                pdoIndex = cellfun(...
                        @(x) EtherCATInfo.hexDecValue(...
                            x.getFirstNode('Index').getTextContent), ...
                        pdo);
                osIndexInc = cellfun(...
                        @(x) str2double(x.getAttribute('OSIndexInc')), ...
                        pdo);

                for j = 1:numel(SlaveConfig.sm{i}{3})
                    n = find(pdoIndex == SlaveConfig.sm{i}{3}{j}{1},1);
                    obj.testPdoEntry(i,j,SlaveConfig.sm{i}{3}{j}{2}, ...
                                 pdo{n},osIndexInc(n));
                end

                txt = cellfun(@(x) x.getAttribute('Mandatory'), pdo, ...
                              'UniformOutput', false);
                mandatory = str2double(txt);
                x = isnan(mandatory);
                mandatory(x) = 0;
                mandatory = (mandatory | strcmp(txt,'true')) ...
                        & cellfun(@(x) str2double(x.getAttribute('Sm')) == SlaveConfig.sm{i}{1}, ...
                                   pdo);

                mandatory = cellfun(@(x) EtherCATInfo.hexDecValue(x.getFirstNode('Index').getTextContent), ...
                                    pdo(mandatory));

                missing = setdiff(mandatory,...
                                  cellfun(@(x) x{1}, SlaveConfig.sm{i}{3}));

                if ~isempty(missing)
                    x = cell2mat(strcat('#x',cellstr(dec2hex(missing)),',')');
                    x(end) = [];
                    fprintf('Mandatory pdo''s %s have not been mapped\n',x)
                end
            end
        end

        %------------------------------------------------------------------
        function testPdoEntry(obj,i,j,sc,pdo,OSIndexInc)
            if isempty(pdo)
                fprintf('SlaveConfig.sm{%i}{3}{%i}: Pdo does not exist\n', ...
                        i,j)
                return
            end

            entry = pdo.getNodes('Entry');

            if isnan(OSIndexInc)
                os_count = 1;
                OSIndexInc = 0;
            else
                os_count = floor(size(sc,1) / numel(entry));
            end

            for k = 1:size(sc,1)
                os_idx = floor((k-1) / numel(entry));
                n = k - os_idx * numel(entry);

                if numel(entry) < n
                    fprintf('SlaveConfig.sm{%i}{3}{%i}{2}(%i,:): Pdo does not have Entry[%i]\n', ...
                        i,j,k,k);
                    continue
                end

                index = EtherCATInfo.hexDecValue(entry{n}.getFirstNode('Index').getTextContent) + os_idx*OSIndexInc;
                bitLen = str2double(entry{n}.getFirstNode('BitLen').getTextContent);
                if sc(k,1) ~= index
                    fprintf('Index SlaveConfig.sm{%i}{3}{%i}{2}(%i,1)=#x%x is incorrect, xml=#x%x\n', ...
                        i,j,k, sc(k,1), index);
                end

                if sc(k,3) ~= bitLen
                    fprintf('BitLen SlaveConfig.sm{%i}{3}{%i}{2}(%i,1)=%i is incorrect, xml=%i\n', ...
                        i,j,k, sc(k,3), bitLen);
                end

                if ~index
                    continue
                end

                subIndex = EtherCATInfo.hexDecValue(entry{n}.getFirstNode('SubIndex').getTextContent);
                if sc(k,2) ~= subIndex
                    fprintf('SubIndex SlaveConfig.sm{%i}{3}{%i}{2}(%i,2)=%i is incorrect, xml=%i\n', ...
                        i,j,k, sc(k,2), subIndex);
                end
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Access = private)
        doc
    end
end
