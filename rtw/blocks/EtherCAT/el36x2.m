%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Encapsulation for oversampling analog input slave EL36x2
%
% Copyright (C) 2018 Richard Hacker
% License: GPLv3+
%
classdef el36x2 < EtherCATSlave

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
methods
%====================================================================
    function obj = el36x2(id)
        if nargin
            obj.slave = obj.find(id);
        end
    end

    %========================================================================
    function rv = configure(obj,one_ch,vector,status,scaling,filter,range)

        % General information
        rv.SlaveConfig.vendor = 2;
        rv.SlaveConfig.product = obj.slave{2};
        rv.SlaveConfig.description = obj.slave{1};

        if one_ch
            channels = 1;
        else
            channels = 1:2;
        end

        % output syncmanager
        rv.SlaveConfig.sm = {{3,1,el36x2.pdos(channels)}};

        % Parameters
        % scaling is a 2x2 matrix
        rv.Parameter = struct('name', {}, 'mask_index', {});

        % row 1 is gain
        if scaling(1,1)
            rv.Parameter(end+1) = struct(...
               'name', 'Gain', 'mask_index', scaling(1,2));

            if vector
                gainparam = struct('parameter', 0);
            else
                gainparam = arrayfun(...
                    @(i) struct('parameter', 0, ...
                                'element', i-1), ...
                    channels,...
                    'UniformOutput', false);
            end
        else
            if vector
                gainparam = [];
            else
                gainparam = repmat({[]}, 1, numel(channels));
            end
        end

        % row 2 is offset
        if scaling(2,1)
            rv.Parameter(end+1) = struct(...
               'name', 'Offset', 'mask_index', scaling(2,2));

            if vector
                offsetparam = struct('parameter', 1);
            else
                offsetparam = arrayfun(...
                    @(i) struct('parameter', 1, ...
                                'element', i-1), ...
                    channels,...
                    'UniformOutput', false);
            end
        else
            if vector
                offsetparam = [];
            else
                offsetparam = gainparam;
            end
        end

        if numel(rv.Parameter)
            fs = 2^31;
        else
            fs = [];
        end

        % Port configuration
        if vector
            rv.PortConfig.output = ...
                struct('pdo',cell2mat(arrayfun(@(i) [0,i-1,8,0], channels, ...
                                      'UniformOutput',0)'), ...
                       'pdo_data_type',sint(32),...
                       'full_scale',fs,...
                       'gain', gainparam, ...
                       'offset', offsetparam);
            if status
                rv.PortConfig.output(end+1) = ...
                    struct('pdo',cell2mat(arrayfun(@(i) [0,i-1,4,0], channels, ...
                                          'UniformOutput',0)'), ...
                           'pdo_data_type',uint(1),...
                           'full_scale',[],...
                           'gain', [], ...
                           'offset', []);
           end
        else
            rv.PortConfig.output = arrayfun( ...
                @(i) struct('pdo',[0,i-1,8,0], ...
                            'pdo_data_type',sint(32),...
                            'full_scale',fs,...
                            'gain', gainparam{i}, ...
                            'offset', offsetparam{i}, ...
                            'portname', strcat('Ch.',num2str(i))), ...
                channels);

            if status
                rv.PortConfig.output(end+1:end+numel(channels)) = arrayfun( ...
                    @(i) struct('pdo',[0,i-1,4,0], ...
                                'pdo_data_type',uint(1),...
                                'full_scale',[],...
                                'gain', [], ...
                                'offset', [], ...
                                'portname', strcat('St.',num2str(i))), ...
                    channels);
            end
        end

        % Slave configuration
        rv.sdo = {hex2dec('8000'), hex2dec('15'), 32, filter};
        if obj.slave{4}
            rv.sdo(end+1,:) = {hex2dec('8000'), hex2dec('19'), 32, range(1)};
            if ~one_ch
                rv.sdo(end+1,:) = {hex2dec('8010'), hex2dec('19'), 32, range(2)};
            end
        end
    end
end

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
methods (Static)
    %========================================================================
    function modelChanged
        obj = el36x2(get_param(gcbh,'model'));                              
                                                                            
        obj.updateRevision();                                               
        obj.channelCountChanged();                                               
    end

    %========================================================================
    function channelCountChanged
        obj = el36x2(get_param(gcbh,'model'));                              
                                                                            
        sdo = {'x8000_15'};
        if obj.slave{4}
            sdo(end+1) = {'x8000_19'};
            if strcmp(get_param(gcbh,'one_ch'), 'off')
                sdo(end+1) = {'x8010_19'};
            end
        end
        EtherCATSlave.updateSDOEnable(sdo);
    end

    %====================================================================
    function test(p)
        ei = EtherCATInfo(fullfile(p,'Beckhoff EL3xxx.xml'));
        for i = 1:size(el36x2.models,1)
            fprintf('Testing %s\n', el36x2.models{i,1});
            slave = ei.getSlave(el36x2.models{i,2},...
                    'revision', el36x2.models{i,3});
            model = el36x2.models{i,1};

            for j = 1:14
                rv = el36x2(model).configure(j&1,j&1,j&1,[0,6;1,7],2,[2 2]);
                slave.testConfig(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end
end     % methods

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (Constant)
    %  name          product code         basic_version       range
    models = {...
      'EL3602',      hex2dec('0e123052'), hex2dec('00130000'), 1;
      'EL3602-0002', hex2dec('0e123052'), hex2dec('00120002'), 0;
      'EL3602-0010', hex2dec('0e123052'), hex2dec('0013000a'), 0;
      'EL3602-0020', hex2dec('0e123052'), hex2dec('00120014'), 0;
      'EL3612',      hex2dec('0e1c3052'), hex2dec('00120000'), 1;
      'EL3612-0020', hex2dec('0e1c3052'), hex2dec('00120014'), 0;
    };
end

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (Access = private, Constant)
    % ADC and status PDO
    pdos = { {hex2dec('1a00'), [hex2dec('6000'),  1,  1;
                                hex2dec('6000'),  2,  1;
                                hex2dec('6000'),  3,  2;
                                hex2dec('6000'),  5,  2;
                                hex2dec('6000'),  7,  1;
                                0              ,  0,  7;
                                hex2dec('1800'),  7,  1;
                                hex2dec('1800'),  9,  1;
                                hex2dec('6000'), 17, 32]},
             {hex2dec('1a01'), [hex2dec('6010'),  1,  1;
                                hex2dec('6010'),  2,  1;
                                hex2dec('6010'),  3,  2;
                                hex2dec('6010'),  5,  2;
                                hex2dec('6010'),  7,  1;
                                0              ,  0,  7;
                                hex2dec('1801'),  7,  1;
                                hex2dec('1801'),  9,  1;
                                hex2dec('6010'), 17, 32]}};

end     % properties

end     % classdef
