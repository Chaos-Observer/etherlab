%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Encapsulation for Power Supply slaves EL9xxx
%
% Copyright (C) 2013 Richard Hacker
% License: LGPL
%
classdef el9xxx < EtherCATSlave

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
methods (Static)
    %========================================================================
    function rv = configure(model,vector)
        slave = EtherCATSlave.findSlave(model,el9xxx.models);

        % General information
        rv.SlaveConfig.vendor = 2;
        rv.SlaveConfig.product = slave{2};
        rv.SlaveConfig.description = slave{1};

        % Get the model's PDO
        pdo = el9xxx.pdo{slave{3}};

        % Input syncmanager
        rv.SlaveConfig.sm = {{0, 1, ...
                cellfun(@(x) {x{1} cell2mat(x{2}(:,1:3))}, pdo, ...
                        'UniformOutput', false)}};

        % Create a cell array with 5 columns:
        %       1: Sm Idx
        %       2: PDO Idx
        %       3: Entry Idx
        %       4: Value Idx
        %       5: PDO Entry name
        pdo_entries = arrayfun(...
                @(i) horzcat(repmat({0,i-1}, size(pdo{i}{2},1), 1), ...
                             num2cell(0:size(pdo{i}{2},1)-1)', ...
                             num2cell(zeros(size(pdo{i}{2},1),1)), ...
                             pdo{i}{2}(:,4)), ...
                1:numel(pdo), 'UniformOutput', false);
        pdo_entries = vertcat(pdo_entries{:});

        if vector
            rv.PortConfig.output = struct(...
                'pdo', cell2mat(pdo_entries(:,1:4)), ...
                'pdo_data_type', uint(1), ...
                'portname','Out' ...
            );
        else    % vector
            rv.PortConfig.output = struct(...
                'pdo', arrayfun(@(x) [pdo_entries{x,1:4}], ...
                                1:size(pdo_entries,1), ...
                                'UniformOutput', false), ...
                'pdo_data_type', uint(1), ...
                'portname', pdo_entries(:,5)' ...
            );
        end     % vector
    end

    %====================================================================
    function test(p)
        ei = EtherCATInfo(fullfile(p,'Beckhoff EL9xxx.xml'));
        for i = 1:size(el9xxx.models,1)-1
            fprintf('Testing %s\n', el9xxx.models{i,1});
            rv = el9xxx.configure(el9xxx.models{i,1},i&1);
            ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
        end
    end
end     % methods

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (Constant, Access=private)
    % PDO class
    pdo = { {{hex2dec('1a00'), {hex2dec('6000'), 1, 1, 'Pwr OK'}}}, ...
            {{hex2dec('1a00'), {hex2dec('6000'), 1, 1, 'Pwr OK'}},  ...
             {hex2dec('1a01'), {hex2dec('6010'), 1, 1, 'FuseErr'}}}, ...
            {{hex2dec('1a00'), {hex2dec('6000'), 1, 1, 'Stat Us'}},  ...
             {hex2dec('1a01'), {hex2dec('6010'), 1, 1, 'Stat Up'}}}, ...
            {{hex2dec('1a00'), {hex2dec('6000'), 1, 1, 'Pwr OK';
                                hex2dec('6000'), 2, 1, 'Overload'}}}, ...
            {{hex2dec('1a00'), {hex2dec('6000'), 1, 1, 'Stat Us';
                                hex2dec('6000'), 2, 1, 'Stat Uo'}}}  ...
          };

end     % properties

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (Constant)
    %  name          product code         PDO class
    models = {...
      'EL9110',      hex2dec('23963052'), 1;
      'EL9160',      hex2dec('23c83052'), 1;
      'EL9210',      hex2dec('23fa3052'), 2;
      'EL9210-0020', hex2dec('23fa3052'), 2;
      'EL9260',      hex2dec('242c3052'), 2;
      'EL9410',      hex2dec('24c23052'), 3;
      'EL9505',      hex2dec('25213052'), 4;
      'EL9508',      hex2dec('25243052'), 4;
      'EL9510',      hex2dec('25263052'), 4;
      'EL9512',      hex2dec('25283052'), 4;
      'EL9515',      hex2dec('252b3052'), 4;
      'EL9560',      hex2dec('25583052'), 5;
    };
end     % properties

end     % classdef
