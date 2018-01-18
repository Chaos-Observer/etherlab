function blkStruct = slblocks

% Information for "Blocksets and Toolboxes" subsystem
blkStruct.Name = sprintf('Embedded Target\n for EtherLab');
blkStruct.OpenFcn = 'etherlab_lib';
blkStruct.MaskDisplay = 'disp(''EtherLab'')';

% Information for Simulink Library Browser
Browser(1).Library = 'etherlab_lib';
Browser(1).Name    = 'Embedded Target for EtherLab';
Browser(1).IsFlat  = 0;

blkStruct.Browser = Browser;
