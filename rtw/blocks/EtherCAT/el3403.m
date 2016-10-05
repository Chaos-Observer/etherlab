classdef el3403 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods
        %====================================================================
        function obj = el3403(id)
            if nargin > 0
                obj.slave = obj.find(id);
            end
        end

        %====================================================================
        function rv = configure(obj,variants,current,voltage,sdo)

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = obj.slave{1};
            rv.SlaveConfig.product  = obj.slave{2};

            rv.SlaveConfig.sm = obj.sm;

            rv.PortConfig.output(1).portname = 'Current';
            rv.PortConfig.output(1).pdo = [1,0,2,0; 1,1,2,0; 1,2,2,0];
            rv.PortConfig.output(1).pdo_data_type = sint(32);
            rv.PortConfig.output(1).gain = current(1)*obj.slave{4}(1,:);

            rv.PortConfig.output(2).portname = 'Voltage';
            rv.PortConfig.output(2).pdo = [1,0,3,0; 1,1,3,0; 1,2,3,0];
            rv.PortConfig.output(2).pdo_data_type = sint(32);
            rv.PortConfig.output(2).gain = voltage(1)*obj.slave{4}(2,:);

            rv.PortConfig.output(3).portname = 'Power';
            rv.PortConfig.output(3).pdo = [1,0,4,0; 1,1,4,0; 1,2,4,0];
            rv.PortConfig.output(3).pdo_data_type = sint(32);
            rv.PortConfig.output(3).gain = current(1)*voltage(1)*obj.slave{4}(3,:);

            rv.PortConfig.output(4).portname = 'Zero Crossing Fail';
            rv.PortConfig.output(4).pdo = [1,3,1,0; 1,3,2,0; 1,3,3,0];
            rv.PortConfig.output(4).pdo_data_type = uint(1);

            rv.PortConfig.output(5).portname = 'Seq. Fail';
            rv.PortConfig.output(5).pdo = [1,3,5,0];
            rv.PortConfig.output(5).pdo_data_type = uint(1);

            if variants
                rv.PortConfig.output(6).portname = 'Index';
                rv.PortConfig.output(6).pdo = [1,0,5,0; 1,1,5,0; 1,2,5,0];
                rv.PortConfig.output(6).pdo_data_type = uint(8);

                rv.PortConfig.output(7).portname = 'Variant';
                rv.PortConfig.output(7).pdo = [1,0,7,0; 1,1,7,0; 1,2,7,0];
                rv.PortConfig.output(7).pdo_data_type = sint(32);

                rv.PortConfig.input.portname = 'Index';
                rv.PortConfig.input.pdo = [0,0,0,0; 0,1,0,0; 0,2,0,0];
                rv.PortConfig.input.pdo_data_type = uint(8);
            else
                rv.PortConfig.input = repmat(struct('portname',[]), 0, 1);
            end

            rv.SlaveConfig.sdo = {
                hex2dec('f800'),  1,  8, sdo(1);
                hex2dec('f800'),  2,  8, sdo(2);
                hex2dec('f800'),  3,  8, sdo(3);
                hex2dec('f800'),  5,  8, sdo(4);
                hex2dec('f800'), 14,  8, sdo(5);
                hex2dec('f800'), 33, 16, sdo(6);
                hex2dec('f800'), 34, 16, sdo(7)};
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL34xx.xml'));
            for i = 1:size(el3403.models,1)
                fprintf('Testing %s\n', el3403.models{i,1});
                slave = ei.getSlave(el3403.models{i,2},...
                        'revision', el3403.models{i,3});

                rv = el3403(el3403.models{i,1}).configure(0,1,1,ones(7,1));
                slave.testConfig(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)
        sm = {
            {2,0,{{hex2dec('1600'), [hex2dec('7000'), 1, 8]};
                  {hex2dec('1601'), [hex2dec('7010'), 1, 8]};
                  {hex2dec('1602'), [hex2dec('7020'), 1, 8]}}},
            {3,1,{{hex2dec('1a00'), [              0,  0, 15;
                                     hex2dec('6000'), 16,  1;
                                     hex2dec('6000'), 17, 32;
                                     hex2dec('6000'), 18, 32;
                                     hex2dec('6000'), 19, 32;
                                     hex2dec('6000'), 20,  8;
                                                   0,  0,  8;
                                     hex2dec('6000'), 29, 32]},
                  {hex2dec('1a01'), [              0,  0, 15;
                                     hex2dec('6010'), 16,  1;
                                     hex2dec('6010'), 17, 32;
                                     hex2dec('6010'), 18, 32;
                                     hex2dec('6010'), 19, 32;
                                     hex2dec('6010'), 20,  8;
                                                   0,  0,  8;
                                     hex2dec('6010'), 29, 32]},
                  {hex2dec('1a02'), [              0,  0, 15;
                                     hex2dec('6020'), 16,  1;
                                     hex2dec('6020'), 17, 32;
                                     hex2dec('6020'), 18, 32;
                                     hex2dec('6020'), 19, 32;
                                     hex2dec('6020'), 20,  8;
                                                   0,  0,  8;
                                     hex2dec('6020'), 29, 32]},
                  {hex2dec('1a03'), [              0,  0,  3;
                                     hex2dec('f100'),  4,  1;
                                     hex2dec('f100'),  5,  1;
                                     hex2dec('f100'),  6,  1;
                                                   0,  0,  2;
                                     hex2dec('f100'),  9,  1;
                                                   0,  0,  7]}}}
        };

        %   Model        ProductCode          Revision      Function  Multiplier Matrix
        models = {
          'EL3403',      hex2dec('0d4b3052'), hex2dec('00160000'), repmat([  1e-6; 0.1e-3; 10e-3], 1, 3);
          'EL3403-0010', hex2dec('0d4b3052'), hex2dec('0017000a'), repmat([  5e-6; 0.1e-3; 10e-3], 1, 3);
          'EL3403-0026', hex2dec('0d4b3052'), hex2dec('0017001a'), repmat([  1e-6; 0.1e-3; 10e-3], 1, 3);
          'EL3403-0100', hex2dec('0d4b3052'), hex2dec('00170064'), repmat([0.1e-6; 0.1e-3;  1e-3], 1, 3);
          'EL3403-0111', hex2dec('0d4b3052'), hex2dec('0017006f'), [1e-6, 0.1e-6, 0.01e-6; 0.1e-3, 0.1e-3, 0.1e-3; 10e-3, 1e-3, 0.1e-3];
          'EL3403-0126', hex2dec('0d4b3052'), hex2dec('0017007e'), [1e-6, 0.1e-6, 0.01e-6; 0.1e-3, 0.1e-3, 0.1e-3; 10e-3, 1e-3, 0.1e-3];
          %'EL3403-0333', hex2dec('0d4b3052'), hex2dec('0017014d'), ones(3,3);    % FIXME
        };
    end
end
