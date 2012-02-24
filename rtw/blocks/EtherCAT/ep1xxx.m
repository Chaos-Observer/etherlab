function rv = ep1xxx(model,output_type)

entries = [...
           hex2dec('6000'),  1, 1, 1001; ...
           hex2dec('6010'),  1, 1, 1001; ...
           hex2dec('6020'),  1, 1, 1001; ...
           hex2dec('6030'),  1, 1, 1001; ...
           hex2dec('6040'),  1, 1, 1001; ...
           hex2dec('6050'),  1, 1, 1001; ...
           hex2dec('6060'),  1, 1, 1001; ...
           hex2dec('6070'),  1, 1, 1001; ...
           hex2dec('6080'),  1, 1, 1001; ...
           hex2dec('6090'),  1, 1, 1001; ...
           hex2dec('60a0'),  1, 1, 1001; ...
           hex2dec('60b0'),  1, 1, 1001; ...
           hex2dec('60c0'),  1, 1, 1001; ...
           hex2dec('60d0'),  1, 1, 1001; ...
           hex2dec('60e0'),  1, 1, 1001; ...
           hex2dec('60f0'),  1, 1, 1001; ...

           hex2dec('6000'),  1, 1, 1001; ...
           hex2dec('6000'),  2, 1, 1001; ...
           hex2dec('6000'),  3, 1, 1001; ...
           hex2dec('6000'),  4, 1, 1001; ...
           hex2dec('6000'),  5, 1, 1001; ...
           hex2dec('6000'),  6, 1, 1001; ...
           hex2dec('6000'),  7, 1, 1001; ...
           hex2dec('6000'),  8, 1, 1001; ...

           hex2dec('6010'),  1, 1, 1001; ...
           hex2dec('6010'),  2, 1, 1001; ...
           hex2dec('6010'),  3, 1, 1001; ...
           hex2dec('6010'),  4, 1, 1001; ...
           hex2dec('6010'),  5, 1, 1001; ...
           hex2dec('6010'),  6, 1, 1001; ...
           hex2dec('6010'),  7, 1, 1001; ...
           hex2dec('6010'),  8, 1, 1001; ...

           
           ];


        

pdo = [...
         hex2dec('1a00'),  1,  1;...
         hex2dec('1a01'),  1,  1;...
         hex2dec('1a02'),  1,  1;...
         hex2dec('1a03'),  1,  1;...
         hex2dec('1a04'),  1,  1;...
         hex2dec('1a05'),  1,  1;...
         hex2dec('1a06'),  1,  1;...
         hex2dec('1a07'),  1,  1;...
         hex2dec('1a08'),  1,  1;...
         hex2dec('1a09'),  1,  1;...
         hex2dec('1a0a'),  1,  1;...
         hex2dec('1a0b'),  1,  1;...
         hex2dec('1a0c'),  1,  1;...
         hex2dec('1a0d'),  1,  1;...
         hex2dec('1a0e'),  1,  1;...
         hex2dec('1a0f'),  1,  1;...

         hex2dec('1a00'), 17, 24;...
         hex2dec('1a01'), 25  32;...
      ];



%   Model                  ProductCode          Revision       TxStart|TxEnd

models = struct(...
    'EP10080001_00100001'  ,[hex2dec('03f04052'), hex2dec('00100001'),  1,   8 ], ...
    'EP10080001_00110001'  ,[hex2dec('03f04052'), hex2dec('00110001'),  1,   8 ], ...
    'EP10080002_00100002'  ,[hex2dec('03f04052'), hex2dec('00100002'),  1,   8 ], ...
    'EP10080002_00110002'  ,[hex2dec('03f04052'), hex2dec('00110002'),  1,   8 ], ...
    'EP10180001_00100001'  ,[hex2dec('03fa4052'), hex2dec('00100001'),  1,   8 ], ...
    'EP10180001_00110001'  ,[hex2dec('03fa4052'), hex2dec('00110001'),  1,   8 ], ...
    'EP10180002_00100002'  ,[hex2dec('03fa4052'), hex2dec('00100002'),  1,   8 ], ...
    'EP10180002_00110002'  ,[hex2dec('03fa4052'), hex2dec('00110002'),  1,   8 ], ...
    'EP10980002_00100001'  ,[hex2dec('044a4052'), hex2dec('00100001'),  1,   8 ], ...
    'EP10980002_00110001'  ,[hex2dec('044a4052'), hex2dec('00110001'),  1,   8 ], ...
    'EP18090021_00110015'  ,[hex2dec('07114052'), hex2dec('00110015'),  1,  16 ], ...
    'EP18090022_00110016'  ,[hex2dec('07114052'), hex2dec('00110016'),  1,  16 ], ...
    'EP18190021_00100015'  ,[hex2dec('071b4052'), hex2dec('00100015'),  1,  16 ], ...
    'EP18190022_00100016'  ,[hex2dec('071b4052'), hex2dec('00100016'),  1,  16 ] ...
    );

rv.SlaveConfig.vendor = 2;
rv.SlaveCongig.description = model;

product = models.(model);
rv.SlaveConfig.product = product(1);

% RxPdo SyncManager
rv.SlaveConfig.sm = { {1 1 {}} };

% Populate the RxPDO Outputs structure
rv.SlaveConfig.sm{1}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        product(3):product(4), ...
        'UniformOutput',0 ...
);

%Populate the Port entries

r = 0 : product(4) - product(3);
if strcmp(output_type, 'Vector Output')
    rv.PortConfig.output.pdo = [zeros(numel(r),4)];
    rv.PortConfig.output.pdo(:,2) = r;
else
    rv.PortConfig.output = arrayfun(@(x) struct('pdo', [0, x 0, 0]), r);
end

