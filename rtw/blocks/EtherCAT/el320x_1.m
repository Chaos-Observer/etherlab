classdef el320x_1 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = configure(model,status,vector,filter,...
                                element,technology,resistance)
            slave = el320x_1.findSlave(model,el320x_1.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};
            rv.function = slave{4};

            pdo_count = str2double(model(6));

            pdo = el320x_1.pdo;
            rv.SlaveConfig.sm = {{3,1, arrayfun(@(i) pdo(i,:), ...
                                         1:pdo_count,...
                                         'UniformOutput', false)}};

            pdo = repmat([0,0,8,0],pdo_count,1);
            pdo(:,2) = 0:pdo_count-1;

            rv.PortConfig.output = ...
                el320x_1.configurePorts('Value',pdo,sint(16),vector);

            if status
                pdo(:,3) = 4;

                if vector
                    n = 1;
                else
                    n = pdo_count;
                end
                
                rv.PortConfig.output(end+(1:n)) = el320x_1.configurePorts(...
                        'Status',pdo,uint(1),vector);
            end

            if status
                for i = 1:pdo_count
                    rv.PortConfig.output(i).full_scale = slave{4};
                end
            else
                rv.PortConfig.output(1).full_scale = slave{4};
            end
            
            config = repmat([0,hex2dec('19'),16,0;
                             0,hex2dec('1a'),16,0;
                             0,hex2dec('1b'),16,0], ...
                            pdo_count, 1);
            config(:,1) = floor((0:3*pdo_count-1)/3) * 16 + hex2dec('8000');
            config(:,4) = reshape([element(1:pdo_count);
                                   technology(1:pdo_count);
                                   resistance(1:pdo_count)],...
                                  [],1);
            config = [hex2dec('8000') + repmat(0:pdo_count-1,1,3)*16;
                      reshape(repmat(hex2dec({'19','1a','1b'})',pdo_count,1),1,[]);
                      repmat(16,1,pdo_count*3);
                      [element(1:pdo_count), technology(1:pdo_count), resistance(1:pdo_count)]]';
                
            rv.SlaveConfig.sdo = ...
                num2cell( [hex2dec('8000'),hex2dec(' 6'), 8,double(filter > 1);
                           hex2dec('8000'),hex2dec('15'),16,max(0,filter-2);
                           config]);
        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL3xxx.xml'));
            for i = 1:size(el320x_1.models,1)
                fprintf('Testing %s\n', el320x_1.models{i,1});
                rv = el320x_1.configure(el320x_1.models{i,1},i&1,i&2,...
                        1,1:4,1:4,1:4);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)
        %              PdoEntry       EntryIdx, SubIdx, Bitlen
        pdo = { hex2dec('1a00'), [hex2dec('6000'), 1, 1;
                                  hex2dec('6000'), 2, 1;
                                  hex2dec('6000'), 3, 2;
                                  hex2dec('6000'), 5, 2;
                                  hex2dec('6000'), 7, 1;
                                  0              , 0, 7;
                                  hex2dec('1800'), 7, 1;
                                  hex2dec('1800'), 9, 1;
                                  hex2dec('6000'),17,16];
                hex2dec('1a01'), [hex2dec('6010'), 1, 1;
                                  hex2dec('6010'), 2, 1;
                                  hex2dec('6010'), 3, 2;
                                  hex2dec('6010'), 5, 2;
                                  hex2dec('6010'), 7, 1;
                                  0              , 0, 7;
                                  hex2dec('1801'), 7, 1;
                                  hex2dec('1801'), 9, 1;
                                  hex2dec('6010'),17,16];
                hex2dec('1a02'), [hex2dec('6020'), 1, 1;
                                  hex2dec('6020'), 2, 1;
                                  hex2dec('6020'), 3, 2;
                                  hex2dec('6020'), 5, 2;
                                  hex2dec('6020'), 7, 1;
                                  0              , 0, 7;
                                  hex2dec('1802'), 7, 1;
                                  hex2dec('1802'), 9, 1;
                                  hex2dec('6020'),17,16];
                hex2dec('1a03'), [hex2dec('6030'), 1, 1;
                                  hex2dec('6030'), 2, 1;
                                  hex2dec('6030'), 3, 2;
                                  hex2dec('6030'), 5, 2;
                                  hex2dec('6030'), 7, 1;
                                  0              , 0, 7;
                                  hex2dec('1803'), 7, 1;
                                  hex2dec('1803'), 9, 1;
                                  hex2dec('6030'),17,16];
        };

        %   Model   ProductCode           Function NoStatus WithStatus;
        models = {
          'EL3201',       hex2dec('0c813052'), hex2dec('00110000'),  10;
          'EL3201-0010',  hex2dec('0c813052'), hex2dec('0011000a'), 100;
          'EL3201-0020',  hex2dec('0c813052'), hex2dec('00110014'), 100;
          'EL3202',       hex2dec('0c823052'), hex2dec('00110000'),  10;
          'EL3202-0010',  hex2dec('0c823052'), hex2dec('0011000a'), 100;
          'EL3204',       hex2dec('0c843052'), hex2dec('00110000'),  10;
        };
    end
end
