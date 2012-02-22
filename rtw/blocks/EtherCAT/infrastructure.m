function rv = infrastructure(model, diagnose)

entries = [...
           hex2dec('6000'),  1,  1, 1001; ...  %RxPdo's
           hex2dec('6010'),  1,  1, 1001; ...  %RxPdo's
          ];

        
 %                   TxPdoSt TxPdoEnd StatusStart StatusEnd                          
pdo = [...               
        hex2dec('1a00'),  1,    1 ;...
        hex2dec('1a01'),  2,    2 ;...
];
                                                                            
%   Model       ProductCode          Revision            RxSt|RxEnd|SM
models = struct( ...
    'EK1100', [hex2dec('044c2c52') hex2dec('00000000'), 0, 0 ],...
    'EK1110', [hex2dec('04562c52') hex2dec('00000000'), 0, 0 ],...
    'EK1122', [hex2dec('04622c52') hex2dec('00100000'), 0, 0 ],...
    'EL9110', [hex2dec('23963052') hex2dec('00100000'), 1, 1 ],...
    'EL9160', [hex2dec('23c83052') hex2dec('00100000'), 1, 1 ],...
    'EL9210', [hex2dec('23fa3052') hex2dec('00100000'), 1, 2 ],...
    'EL9260', [hex2dec('242c3052') hex2dec('00100000'), 1, 2 ]...
   );

prod_name = struct( ...
    'EK1100', 'EtherCAT Coupler',...    
    'EK1110', 'EtherCAT Extension',...    
    'EK1122', '24V w. Diag.',...    
    'EL9110', '230V w. Diag.',...    
    'EL9160', '24V w. Fuse & Diag.',...    
    'EL9210', '230V w. Fuse & Diag.',...    
    'EL9260', '2-Port Ethercat Junction'... 
);

product = models.(model);            
rv.SlaveConfig.product = product(1); 
rv.SlaveConfig.vendor = 2;
rv.name = prod_name.(model); 

pdoindex = product(3):product(4);


if product(3)
% RxPdo SyncManager
   rv.SlaveConfig.pdo = { {0 0 {}} };;
    
   rv.SlaveConfig.pdo{1}{3} = arrayfun(...
           @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
           pdoindex, ...
           'UniformOutput',0 ...
   );
    
    
   % Populate the block's output port(s)
   r = 0: numel(pdoindex)-1
    
   rv.PortConfig.output = arrayfun(@(x) struct('pdo', [0, x, 0, 0]), r);
else
    rv.PortConfig = [];
end


