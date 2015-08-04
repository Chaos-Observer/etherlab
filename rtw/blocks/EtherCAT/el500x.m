%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% SSI decoder EL5001, EL5001-0011, EL5002
%
% Copyright (C) 2013 Richard Hacker
% License: LGPL
%
classdef el500x < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function updateModel
            slave = EtherCATSlave.findSlave(get_param(gcbh,'model'),...
                                            el500x.models);
            EtherCATSlaveBlock.setVisible('pdo_x1A02', slave{5});
            EtherCATSlaveBlock.updateSDOVisibility(...
                dec2base(slave{6},10,2));
        end

        %====================================================================
        function rv = configure(model,timestamp,sdo_config,dc_config)
            slave = EtherCATSlave.findSlave(model,el500x.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            pdo_list = el500x.pdo;

            % Get a list of pdo's for the selected slave
            selected = boolean(zeros(1,length(pdo_list)));
            selected(slave{4}) = 1;

            % Add timestamp pdo
            if slave{5} && timestamp
                selected(slave{5}) = 1;
            end
            
            % Configure SM3
            tx = find(selected);
            rv.SlaveConfig.sm = ...
                 {{3,1, arrayfun(@(x) {pdo_list{x,1}, pdo_list{x,2}},...
                                 tx, 'UniformOutput', false)}};

            % Configure output port. The algorithm below will group all boolean
            % signals to one port. All other entries get a separate port
            outputs = arrayfun(@(i) arrayfun(@(j) {i-1, ...
                                                   pdo_list{tx(i),2}(1,3), ...
                                                   pdo_list{tx(i),3}{j,1}',...
                                                   pdo_list{tx(i),3}{j,2}},...
                                            1:size(pdo_list{tx(i),3},1),...
                                            'UniformOutput', false), ...
                              1:numel(tx), 'UniformOutput', false);
            outputs = horzcat(outputs{:});

            rv.PortConfig.output = ...
                cellfun(@(i) struct('pdo',horzcat(repmat([0,i{1}], ...
                                                         size(i{3})), ...
                                                  i{3}, zeros(size(i{3}))), ...
                                    'pdo_data_type', uint(i{2}), ...
                                    'portname', i{4}), ...
                         outputs);

            % Distributed clocks
            if dc_config(1) == 5
                % Custom
                rv.SlaveConfig.dc = dc_config(2:11);
            elseif dc_config(1) > 1 && ismember(dc_config(1), slave{7})
                % Preconfigured
                dc = el500x.dc;
                rv.SlaveConfig.dc = dc(dc_config(1),:);
            end

            % CoE Configuration
            sdo = el500x.sdo;
            rv.SlaveConfig.sdo = num2cell([sdo(slave{6},:), ...
                                           sdo_config(slave{6})']);
        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL5xxx.xml'));
            for i = 1:size(el500x.models,1)
                fprintf('Testing %s\n', el500x.models{i,1});
                pdo = el500x.pdo;
                rv = el500x.configure(el500x.models{i,1}, i&1,1:21,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Access = private)

        %              PdoEntry       EntryIdx, SubIdx, Bitlen
        pdo = { hex2dec('1a00') [hex2dec('6000'), 1, 1;
                                 hex2dec('6000'), 2, 1;
                                 hex2dec('6000'), 3, 1;
                                 0              , 0,10;
                                 hex2dec('1c32'),32, 1;
                                 hex2dec('1800'), 7, 1;
                                 hex2dec('1800'), 9, 1;
                                 hex2dec('6000'),17,32], ...
                        { 7, 'Counter'; 0:2, 'Status' };
                hex2dec('1a01') [hex2dec('6010'), 1, 1;
                                 hex2dec('6010'), 2, 1;
                                 hex2dec('6010'), 3, 1;
                                 0              , 0,10;
                                 hex2dec('1c32'),32, 1;
                                 hex2dec('1801'), 7, 1;
                                 hex2dec('1801'), 9, 1;
                                 hex2dec('6010'),17,32],...
                        { 7, 'Counter'; 0:2, 'Status' };
                hex2dec('1a00') [hex2dec('6000'), 1, 1;
                                 hex2dec('6000'), 2, 1;
                                 hex2dec('6000'), 3, 1;
                                 0              , 0, 5;
                                 0              , 0, 6;
                                 hex2dec('6000'),15, 1;
                                 hex2dec('6000'),16, 1;
                                 hex2dec('6000'),17,32],...
                        { 7, 'Counter'; 0:2, 'Status' };
                hex2dec('1a02') [hex2dec('6000'),18,32], ...
                        { 0, 'Time' };
        };

        %% SDO Definition
        %%% EL5001, 8010
        %    '1 1 Disable frame error'
        %    '2 1 Enable power failure bit'
        %    '3 1 Enable inhibit time'
        %    '4 1 Enable test mode'
        %    '6 1 SSI-coding'
        %    '9 3 SSI-baudrate'
        %    '15 2 SSI-frame type'
        %    '17 16 SSI-frame size'
        %    '18 16 SSI-data length'
        %    '19 16 Min. inhibit time[µs]'
        %%% EL5001-0011, #x8000
        %    '1 1 Disable frame error'
        %    '2 1 Enable power failure bit'
        %    '5 1 Check SSI-frame size'
        %    '6 1 SSI-coding'
        %    '15 2 SSI-frame type'
        %    '17 16 SSI-frame size'
        %    '18 16 SSI-data length'
        %%% EL5002, #x8000, #x8010
        %    '1 1 Disable frame error'
        %    '2 1 Enable power failure bit'
        %    '3 1 Enable inhibit time'
        %    '4 1 Enable test mode'
        %    '6 1 SSI-coding'
        %    '9 3 SSI-baudrate'
        %    '15 2 SSI-frame type'
        %    '17 16 SSI-frame size'
        %    '18 16 SSI-data length'
        %    '19 16 Min. inhibit time[µs]'

        sdo = [ % #x8000: EL5001-0011 EL5002
                hex2dec('8000'),  1,  8;    % Disable frame error
                hex2dec('8000'),  2,  8;    % Enable power failure bit
                hex2dec('8000'),  3,  8;    % Enable inhibit time (EL5002)
                hex2dec('8000'),  4,  8;    % Enable test mode (EL5002)
                hex2dec('8000'),  5,  8;    % Check SSI-frame size (EL5001-001x
                hex2dec('8000'),  6,  8;    % SSI-coding
                hex2dec('8000'),  9,  8;    % SSI-baudrate (EL5002)
                hex2dec('8000'), 15,  8;    % SSI-frame type
                hex2dec('8000'), 17, 16;    % SSI-frame size
                hex2dec('8000'), 18, 16;    % SSI-data length
                hex2dec('8000'), 19, 16;    % Min. inhibit time[µs] (EL5002)

                % #x8010: EL5001, EL5002
                hex2dec('8010'),  1,  8;    % Disable frame error
                hex2dec('8010'),  2,  8;    % Enable power failure bit
                hex2dec('8010'),  3,  8;    % Enable inhibit time
                hex2dec('8010'),  4,  8;    % Enable test mode
                hex2dec('8010'),  6,  8;    % SSI-coding
                hex2dec('8010'),  9,  8;    % SSI-baudrate
                hex2dec('8010'), 15,  8;    % SSI-frame type
                hex2dec('8010'), 17, 16;    % SSI-frame size
                hex2dec('8010'), 18, 16;    % SSI-data length
                hex2dec('8010'), 19, 16;    % Min. inhibit time[µs]
        ];

        dc = [           0  ,0,0,0,0,0,0,1,    0,0; % FreeRun
              hex2dec('700'),0,1,0,0,0,0,1,15000,0; % DC-Synchron
              hex2dec('700'),0,1,0,0,1,0,1,15000,0; % DC-Synchron (input based)
              hex2dec('120'),0,0,0,0,0,0,1,    0,0];% DC-Latch active (only for EL5001-001x)
    end

    properties (Constant)
        %   Model           ProductCode         RevisionNo
        %    ValueIdx  TimeIdx       SDO
        models = {...
            'EL5001',       hex2dec('13893052'), [],...
                2,     0,            12:21,   1:3;
            'EL5001-0011',  hex2dec('13893052'), [],...
                3,     4, [1,2,5,6,8,9,10],   [1,4];
            'EL5002',       hex2dec('138a3052'), [],...
                [1,2], 0,       [1:4,6:21],   1:3;
        };
    end
end
