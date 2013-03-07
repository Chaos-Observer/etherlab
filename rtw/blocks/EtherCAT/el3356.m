%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Encapsulation for DMS Slave EL3356
%
% Copyright (C) 2013 Richard Hacker
% License: LGPL
%
classdef el3356

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
methods
    %========================================================================
    function rv = getModels(obj)
        rv = obj.models(:,1:3);
    end

    %========================================================================
    function rv = getCodeAndRevision(obj,model)
        row = find(strcmp(obj.models(:,1), model));
        rv = cell2mat(obj.models(row,2:3));
    end

    %========================================================================
    function rv = getDC(obj,model)
        if strcmp(model,'EL3356')
            rv = [];
        else
            rv = obj.dc;
        end
    end

    %========================================================================
    function rv = getSDO(obj,model)
        rv = obj.sdo;
        if strcmp(model,'EL3356')
            rv([2,4,7],:) = [];
        end
    end

    %========================================================================
    function rv = configure(obj,model,adc,dc_spec,scaling,sdo)
        rv = [];

        row = find(strcmp(obj.models(:,1), model));

        % Whether this is EL3356, not EL3356-0010
        % The predefined PDO and DC structures are for EL3356-0010
        % The EL3356 does not have all these features
        el3356 = obj.models{row,4};

        % General information
        rv.SlaveConfig.vendor = 2;
        rv.SlaveConfig.product = obj.models{row,2};
        rv.SlaveConfig.description = obj.models{row,1};

        % Input and output syncmanager
        rv.SlaveConfig.sm = {{2,0,obj.ctrl_pdo}, {3,1,{}}};
        if el3356
            % Remove PDO Entry 7000:04 for EL3356
            rv.SlaveConfig.sm{1}{3}{1}{2}(4,1:2) = [0,0];
        end

        % Control input port
        % simply all pdo entries
        rows = find(rv.SlaveConfig.sm{1}{3}{1}{2}(:,1)) - 1;
        rv.PortConfig.input = struct(...
                'pdo',horzcat(zeros(size(rows,1),2), ...
                              rows, ...
                              zeros(size(rows))), ...
                'pdo_data_type', uint(1), ...
                'portname','Ctrl' ...
        );

        if adc
            % 2xADC Converter
            rv.SlaveConfig.sm{2}{3} = obj.adc_status_pdo;
            if el3356
                % Modifications for EL3356
                % modify entry row 5 [0,0,1] to bitlen 8
                % remove entry row 6 [0,0,7]
                rv.SlaveConfig.sm{2}{3}{1}{2}(5,3) = 8;
                rv.SlaveConfig.sm{2}{3}{1}{2}(6,:) = [];
                rv.SlaveConfig.sm{2}{3}{2}{2}(5,3) = 8;
                rv.SlaveConfig.sm{2}{3}{2}{2}(6,:) = [];
            end

            % Signal output port; only entry with bitlen of 32
            row = find(rv.SlaveConfig.sm{2}{3}{1}{2}(:,3) == 32) - 1;
            rv.PortConfig.output = struct(...
                'pdo', [1,0,row,0;
                        1,1,row,0], ...
                'pdo_data_type', sint(32), ...
                'portname', 'ADC' ...
            );

            % Status 1 output port; all rows with bitlen of 1, except last
            % TxPDO toggle
            row = find(  obj.adc_status_pdo{1}{2}(:,3) == 1 ...
                       & obj.adc_status_pdo{1}{2}(:,1)) - 1;
            row(end) = [];      % Remove TxPDO toggle 60x0:16
            rv.PortConfig.output(2) = struct(...
                'pdo', horzcat(ones(size(row)),...
                               zeros(size(row)), ...
                               row,...
                               zeros(size(row))), ...
                'pdo_data_type', uint(1), ...
                'portname', 'Status 1' ...
            );

            % Status 2 output port; similar to Status 1
            rv.PortConfig.output(3) = rv.PortConfig.output(2);
            rv.PortConfig.output(3).pdo(:,2) = ones(size(row));
            rv.PortConfig.output(3).portname = 'Status 2';

        else
            % Bridge measurement
            rv.SlaveConfig.sm{2}{3} = obj.rmb_status_pdo;

            % Signal output port; only entry of PDO 1a01
            rv.PortConfig.output = struct(...
                'pdo', [1,1,0,0], ...
                'pdo_data_type', sint(32), ...
                'portname', 'RMB' ...
            );

            % Status output port; all entries with bitlen of 1,
            % except for last 2 rows: Sync_error and TxPDO_toggle
            row = find(  obj.rmb_status_pdo{1}{2}(:,3) == 1 ...
                       & obj.rmb_status_pdo{1}{2}(:,1)) - 1;
            row(end-1:end) = [];      % Remove Sync error 1c32:32 
                                      % and TxPDO toggle 1800:9
            rv.PortConfig.output(2) = struct(...
                'pdo', horzcat(ones(size(row)),...
                               zeros(size(row)), ...
                               row,...
                               zeros(size(row))), ...
                'pdo_data_type', uint(1), ...
                'portname', 'Status' ...
            );

        end

        % Distributed clock for EL3356-0010 only
        if ~el3356 && dc_spec(1)
            if dc_spec(1) ~= 4
                % DC Configuration from the default list
                rv.SlaveConfig.dc = obj.dc(dc_spec(1),:);
            else
                % Custom DC
                rv.SlaveConfig.dc = dc_spec(2:end);
            end

            if rv.SlaveConfig.dc(1)
                % Add Timestamp PDO when using DC - required
                n = numel(rv.SlaveConfig.sm{2}{3});
                rv.SlaveConfig.sm{2}{3}{n+1} = obj.time_pdo;

                % Output the timestamp as 2 uint32's
                rv.PortConfig.output(end+1) = struct(...
                    'pdo', [1,n,0,0;
                            1,n,0,1], ...
                    'pdo_data_type', uint(32), ...
                    'portname', 'Time' ...
                );
            end
        end

        % Scaling and filter of output port 1
        if numel(scaling.gain) || numel(scaling.offset)
            rv.PortConfig.output(1).full_scale = 2^31;
        end
        if numel(scaling.gain)
            rv.PortConfig.output(1).gain = {'Gain', scaling.gain};
        end
        if numel(scaling.offset)
            rv.PortConfig.output(1).offset = {'Offset', scaling.offset};
        end
        if numel(scaling.filter)
            rv.PortConfig.output(1).filter = {'Filter', scaling.filter};
        end

        % SDO Configuration
        rv.SlaveConfig.sdo = num2cell(horzcat(obj.sdo, reshape(sdo,[],1)));
        if el3356
            rv.SlaveConfig.sdo([2,4,7],:) = [];
        end

    end
end     % methods

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (SetAccess=private)
    %  name          product code         revision             modify pdo
    models = {...
      'EL3356',      hex2dec('0d1c3052'),  0, true;
      'EL3356-0010', hex2dec('0d1c3052'), 10, false;
    };

    % All known sdo's
    % For EL3356, remove rows 2,4,7
    sdo = [hex2dec('8000'), hex2dec('1'),   8;
           hex2dec('8000'), hex2dec('2'),   8;
           hex2dec('8000'), hex2dec('3'),   8;
           hex2dec('8000'), hex2dec('4'),   8;
           hex2dec('8000'), hex2dec('5'),   8;
           hex2dec('8000'), hex2dec('11'), 16;
           hex2dec('8000'), hex2dec('12'), 16;
           hex2dec('8000'), hex2dec('13'), 16;
           hex2dec('8000'), hex2dec('14'), 32;
           hex2dec('8000'), hex2dec('29'), 16;
           hex2dec('8000'), hex2dec('2a'), 32;
           hex2dec('8000'), hex2dec('31'), 16;
           hex2dec('8000'), hex2dec('32'), 16;
           hex2dec('8010'), hex2dec('06'),  8;
           hex2dec('8010'), hex2dec('15'), 16;
           hex2dec('8020'), hex2dec('06'),  8;
           hex2dec('8020'), hex2dec('15'), 16];

    % Control port pdo
    % for EL3356, PdoEntry 7000:4 must be zeroed
    ctrl_pdo =       { {hex2dec('1600'), [hex2dec('7000'),  1,  1;
                                          hex2dec('7000'),  2,  1;
                                          hex2dec('7000'),  3,  1;
                                          hex2dec('7000'),  4,  1;
                                          hex2dec('7000'),  5,  1;
                                          0              ,  0, 11]} };

    % ADC and status PDO
    % for EL3356, pdo entry 5 must be removed and entry 6 is occupies 8 bits
    adc_status_pdo = { {hex2dec('1a04'), [hex2dec('6010'),  1,  1;
                                          hex2dec('6010'),  2,  1;
                                          0              ,  0,  4;
                                          hex2dec('6010'),  7,  1;
                                          0              ,  0,  1;
                                          0              ,  0,  7;
                                          hex2dec('6010'), 16,  1;
                                          hex2dec('6010'), 17, 32]}, ...
                       {hex2dec('1a06'), [hex2dec('6020'),  1,  1;
                                          hex2dec('6020'),  2,  1;
                                          0              ,  0,  4;
                                          hex2dec('6020'),  7,  1;
                                          0              ,  0,  1;
                                          0              ,  0,  7;
                                          hex2dec('6020'), 16,  1;
                                          hex2dec('6020'), 17, 32]} };

    % RMB only pdo
    rmb_status_pdo = { {hex2dec('1a00'), [0              ,  0,  1;
                                          hex2dec('6000'),  2,  1;
                                          0              ,  0,  1;
                                          hex2dec('6000'),  4,  1;
                                          0              ,  0,  2;
                                          hex2dec('6000'),  7,  1;
                                          hex2dec('6000'),  8,  1;
                                          hex2dec('6000'),  9,  1;
                                          0              ,  0,  4;
                                          hex2dec('1c32'), 32,  1;
                                          0              ,  0,  1;
                                          hex2dec('1800'),  9,  1]}, ...
                       {hex2dec('1a01'), [hex2dec('6000'), 17, 32]} };

    % Clock PDO, required by EL3356-0010 when using DC
    time_pdo =         {hex2dec('1a03'), [hex2dec('6000'), 19, 64]};

    % Distributed Clock (EL3356-0010 only)
    dc = [              0,0,0,0,0,0,0,0,0,0;    % SM-Synchron
           hex2dec('320'),0,1,0,0,0,0,1,0,0;    % DC-Synchron
           hex2dec('320'),0,1,0,0,1,0,1,0,0];   % DC-Synchron (input based)
end     % properties

end     % classdef
