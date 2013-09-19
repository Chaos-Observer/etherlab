classdef el31xx_1 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = configure(model,status,vector,scale,dc)
            slave = el31xx_1.findSlave(model,el31xx_1.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};
            rv.function = slave{3};

            pdo_count = str2double(model(6));

            if status
                pdo_idx = slave{5};
                value_idx = 10;
            else
                pdo_idx = slave{4};
                value_idx = 0;
            end

            pdo = el31xx_1.pdo;
            rv.SlaveConfig.sm = {{3,1, arrayfun(@(i) pdo(i,:), ...
                                         pdo_idx:(pdo_idx+pdo_count-1), ...
                                         'UniformOutput', false)}};
            if dc(1) > 3
                rv.SlaveConfig.dc = dc(2:end);
            else
                rv.SlaveConfig.dc = el31xx_1.dc{dc(1),2};
            end

            pdo = repmat([0,0,value_idx,0],pdo_count,1);
            pdo(:,2) = 0:pdo_count-1;

            rv.PortConfig.output = ...
                el31xx_1.configurePorts('Value',pdo,sint(16),vector,scale,15);

            if status
                pdo(:,3) = 4;

                if vector
                    n = 1;
                else
                    n = pdo_count;
                end
                
                rv.PortConfig.output(end+(1:n)) = ...
                        el31xx_1.configurePorts('Status',pdo,uint(1),vector,scale);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)

        %              PdoEntry       EntryIdx, SubIdx, Bitlen
        pdo = { % For all slaves except EL31.2 without status
                hex2dec('1a01'), [hex2dec('6000') , 17, 16];     %1
                hex2dec('1a03'), [hex2dec('6010') , 17, 16];     %2
                hex2dec('1a05'), [hex2dec('6020') , 17, 16];     %3
                hex2dec('1a07'), [hex2dec('6030') , 17, 16];     %4

                % For EL31.2 without status
                hex2dec('1a03'), [hex2dec('6000') , 17, 16];     %5
                hex2dec('1a05'), [hex2dec('6010') , 17, 16];     %6

                % With status
                hex2dec('1a00'), [hex2dec('6000') ,  1,  1;      %7
                                  hex2dec('6000') ,  2,  1;
                                  hex2dec('6000') ,  3,  2;
                                  hex2dec('6000') ,  5,  2;
                                  hex2dec('6000') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  5;
                                  hex2dec('1c32') , 32,  1;
                                  hex2dec('1800') ,  7,  1;
                                  hex2dec('1800') ,  9,  1;
                                  hex2dec('6000') , 17, 16];
                hex2dec('1a02'), [hex2dec('6010') ,  1,  1;      %8
                                  hex2dec('6010') ,  2,  1;
                                  hex2dec('6010') ,  3,  2;
                                  hex2dec('6010') ,  5,  2;
                                  hex2dec('6010') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  5;
                                  hex2dec('1c32') , 32,  1;
                                  hex2dec('1802') ,  7,  1;
                                  hex2dec('1802') ,  9,  1;
                                  hex2dec('6010') , 17, 16];
                hex2dec('1a04'), [hex2dec('6020') ,  1,  1;      %9
                                  hex2dec('6020') ,  2,  1;
                                  hex2dec('6020') ,  3,  2;
                                  hex2dec('6020') ,  5,  2;
                                  hex2dec('6020') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  5;
                                  hex2dec('1c32') , 32,  1;
                                  hex2dec('1804') ,  7,  1;
                                  hex2dec('1804') ,  9,  1;
                                  hex2dec('6020') , 17, 16];
                hex2dec('1a06'), [hex2dec('6030') ,  1,  1;      %10
                                  hex2dec('6030') ,  2,  1;
                                  hex2dec('6030') ,  3,  2;
                                  hex2dec('6030') ,  5,  2;
                                  hex2dec('6030') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  5;
                                  hex2dec('1c32') , 32,  1;
                                  hex2dec('1806') ,  7,  1;
                                  hex2dec('1806') ,  9,  1;
                                  hex2dec('6030') , 17, 16];

                hex2dec('1a02'), [hex2dec('6000') ,  1,  1;      %11
                                  hex2dec('6000') ,  2,  1;
                                  hex2dec('6000') ,  3,  2;
                                  hex2dec('6000') ,  5,  2;
                                  hex2dec('6000') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  5;
                                  hex2dec('1c33') , 32,  1;
                                  hex2dec('1802') ,  7,  1;
                                  hex2dec('1802') ,  9,  1;
                                  hex2dec('6000') , 17, 16];
                hex2dec('1a04'), [hex2dec('6010') ,  1,  1;      %12
                                  hex2dec('6010') ,  2,  1;
                                  hex2dec('6010') ,  3,  2;
                                  hex2dec('6010') ,  5,  2;
                                  hex2dec('6010') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  5;
                                  hex2dec('1c33') , 32,  1;
                                  hex2dec('1804') ,  7,  1;
                                  hex2dec('1804') ,  9,  1;
                                  hex2dec('6010') , 17, 16];

                hex2dec('1a00'), [hex2dec('6000') ,  1,  1;      %13
                                  hex2dec('6000') ,  2,  1;
                                  hex2dec('6000') ,  3,  2;
                                  hex2dec('6000') ,  5,  2;
                                  hex2dec('6000') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  5;
                                  hex2dec('1c33') , 32,  1;
                                  hex2dec('1800') ,  7,  1;
                                  hex2dec('1800') ,  9,  1;
                                  hex2dec('6000') , 17, 16]};


        %   Model   ProductCode           Function NoStatus WithStatus;
        models = {
          'EL3101',      hex2dec('0c1d3052'), '+-10V' , 1,  7;
          'EL3102',      hex2dec('0c1e3052'), '+-10V' , 5, 11;
          'EL3104',      hex2dec('0c203052'), '+-10V' , 1,  7;
          'EL3111',      hex2dec('0c273052'), '0-20mA', 1, 13;
          'EL3112',      hex2dec('0c283052'), '0-20mA', 5, 11;
          'EL3114',      hex2dec('0c2a3052'), '0-20mA', 1,  7;
          'EL3121',      hex2dec('0c313052'), '4-20mA', 1,  7;
          'EL3122',      hex2dec('0c323052'), '4-20mA', 5, 11;
          'EL3124',      hex2dec('0c343052'), '4-20mA', 1,  7;
          'EL3141',      hex2dec('0c453052'), '0-20mA', 1,  7;
          'EL3142',      hex2dec('0c463052'), '0-20mA', 5, 11;
          'EL3142-0010', hex2dec('0c463052'), '+-10mA', 5, 11;
          'EL3144',      hex2dec('0c483052'), '0-20mA', 1,  7;
          'EL3151',      hex2dec('0c4f3052'), '4-20mA', 1,  7;
          'EL3152',      hex2dec('0c503052'), '4-20mA', 5, 11;
          'EL3154',      hex2dec('0c523052'), '4-20mA', 1,  7;
          'EL3161',      hex2dec('0c593052'), '0-10V' , 1,  7;
          'EL3162',      hex2dec('0c5a3052'), '0-10V' , 5, 11;
          'EL3164',      hex2dec('0c5c3052'), '0-10V' , 1,  7;
        };

        dc = {'Free Run/SM-Synchron', repmat(0,10,1);
              'DC-Synchron',  [hex2dec('700'), 0, 1, 0, 0, 0, 0, 1, 5000, 0];
              'DC-Synchron (input based)', ...
                              [hex2dec('700'), 0, 1, 0, 0, 1, 0, 1, 5000, 0];
        };
    end
end
