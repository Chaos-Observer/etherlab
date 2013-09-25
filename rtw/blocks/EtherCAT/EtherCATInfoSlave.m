classdef EtherCATInfoSlave < XmlNode
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods
        %------------------------------------------------------------------
        function slave = EtherCATInfoSlave(s)
            slave = slave@XmlNode(s);
        end

        %------------------------------------------------------------------
        function rv = Name(s)
            type = s.getFirstNode('Type');
            rv = char(type.getTextContent);
        end

        %------------------------------------------------------------------
        function rv = ProductCode(s)
            type = s.getFirstNode('Type');
            pc = type.getAttribute('ProductCode');
            rv = EtherCATInfo.hexDecValue(char(pc));
        end

        %------------------------------------------------------------------
        function rv = hideTypes(s)
            rv = cellfun(@(x) EtherCATInfo.hexDecValue(...
                                x.getAttribute('RevisionNo')), ...
                         s.getNodes('HideType'));
        end

        %------------------------------------------------------------------
        function rv = RevisionNumber(s)
            type = s.getFirstNode('Type');
            pc = type.getAttribute('RevisionNo');
            rv = EtherCATInfo.hexDecValue(char(pc));
        end
    end
end
