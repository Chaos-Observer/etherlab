#!/usr/bin/python3

#-----------------------------------------------------------------------------
#
# $Id$
#
#-----------------------------------------------------------------------------

from lxml import etree
import codecs
import locale
import os
import sys
import argparse

if (sys.stdout.encoding == None):
    sys.stdout = codecs.getwriter(locale.getpreferredencoding())(sys.stdout)

#-----------------------------------------------------------------------------

######################################################################
class DocumentError(Exception): pass

######################################################################
class EtherLabMessagesLookup(etree.ElementBase):
    @property
    def NodeGroup(self):
        return self.tag == 'NodeGroup'

######################################################################
class CodecOpen:
    def __init__(self, mode='r', encoding='UTF-8'):
        self.__mode = mode
        self.__encoding = encoding

    def __call__(self, file):
        return codecs.open(file, self.__mode, self.__encoding)

######################################################################
class EtherLabMessages:
    """Wrap XML file as EtherLabMessages"""

    def __init__(self, filename):

        parser_lookup = etree.ElementDefaultClassLookup(element = EtherLabMessagesLookup)
        parser = etree.XMLParser(remove_blank_text=True)
        parser.set_element_class_lookup(parser_lookup)

        try:
            self.__xml = etree.parse(filename, parser)
            self.__filename = filename
        except etree.ParseError as e:
            print ('{0} is not a valid xml file: {1}'.format(filename, str(e)))
            sys.exit(2)
        except IOError:
            if os.path.isfile(filename):
                raise

            #ans = raw_input("{0} does not exist. Create it? ".format(filename))
            ans = 'y'
            if ans.lower() == 'y':
                self.__filename = filename
            else:
                self.__filename = None

            self.__xml = etree.ElementTree(etree.Element('EtherLabMessages'), parser = parser)

        self.__root = self.__xml.getroot()

    def importTextFile(self, filename):
        flatTypes = {
            'Emergency': 'Error',
            'Alert': 'Error',
            'Critical': 'Error',
            'Error': 'Error',
            'Info': 'Information',
            'Warning': 'Warning',
            'Warn': 'Warning',
            'Info': 'Information',
            'Notice': 'Information'
            }

        leaves = set(EtherLabMessages.__findLeaves(self.__root))

        for line in open(filename).readlines():
            elements = line.strip().split('\t')
            if len(elements) < 3:
                elements.append('1')
            path, level, width = elements #line.split('\t')
            path = path.strip('/')

            width = int(width)

            node = self.__root
            for name in path.split('/'):
                parent = node
                node = self.__findNodeGroup(parent, name)
                if node is None:
                    node = etree.SubElement(parent, 'NodeGroup')
                    node.set('name', name)
                    etree.SubElement(node, 'Node', {'name': name})

            if node.attrib.has_key('deprecated'):
                node.attrib.pop('deprecated')

            if width > 1:
                node.set("width", str(width))
            elif node.attrib.has_key('width'):
                node.attrib.pop('width')

            node.set('type', flatTypes[level])
            leaves.discard(node)

        for i in leaves:
            i.set('deprecated', '1')

        if self.__filename:
            self.__xml.write(self.__filename, encoding = 'utf-8',
                    xml_declaration = True, pretty_print = True)

    def export(self, visitor):
        """Walk through the document to export it. The visitor must present
        a method visitNodeGroup(self, group, path, messages, width), which
        is called for every leaf found.
        The arguments of visitNodeGroup():
            self:  visitor argument passed to export()
            group: A NodeGroup instance representing the group
            path: The path of the node
            messages: The EtherLabPlainMessages instance
            width: Number of elements in group
        """
        try:
            self.__visitLeaf(self.__root, visitor)
        except DocumentError as e:
            raise Exception(
                    "Error processing {0} in line {1}: {2}".format(
                        EtherLabMessages.__nodeGroupPath(e[0]),
                        e[0].sourceline, str(e[1])))

    def test(self):
        self.__testNodeGroup(self.__root)

    def expand(self, node, lang, n):
        """Expand the text node (Instance of XmlElement) with all maps and
        indices to element n"""
        result = node.text or ''

        l = list(node.iterfind('Entry'))
        if l:
            if len(l) < n:
                return ''
            return self.expand(self.getText(l[n], lang), lang, 0)

        for child in node:
            if child.tag == 'Map':
                array = self.findMap(node, child)
                if array is None:
                    raise DocumentError(
                            '<Array key="{0}"> does not exist'.format(
                                child.get('key')))

                result += self.getMapText(array, lang, n)
            elif child.tag == 'Index':
                result += str(int(child.get('offset','1')) + n)

            result += child.tail or ''

        return result

    def languages(self):
        """Return a set of languages"""
        return EtherLabMessages.__getLanguages(self.__root)

    def getText(self, node, lang):
        result = self.__getLangElement('Text', node, lang)
        if result is None:
            raise DocumentError(
                    (
                        'There is no generic <Text> '
                        'or language specific <Text lang="{0}"> element'
                    ).format(lang))
        return result

    def getDescription(self, node, lang):
        result = self.__getLangElement('Description', node, lang)
        if result is None:
            return result
        elif result.text is None:
            return None

        return result.text.strip()

    @staticmethod
    def findMap(node, item):
        """Return an Array() object.
        If item is an instance of XmlElement or etree._Element, the attribute
        'key' is used. Otherwise item is searched for"""

        # Extract key if item is <Map>
        if isinstance(item, etree._Element) and item.tag == 'Map':
            key = item.get('key')
        else:
            key = item

        # Find map in node
        for n in EtherLabMessages.__findArray(node):
            if n.get('key') == key:
                return n

        # Find map in parent node
        parent = node.getparent()
        if parent is not None:
            return EtherLabMessages.findMap(parent, key)

        # No map is found
        return None

    @staticmethod
    def getMapText(node, lang, n):
        entries = EtherLabMessages.__findEntry(node)
        if len(entries) > n:
            result = EtherLabMessages.__getLangElement('Text', entries[n], lang)
            if result is None:
                result = entries[n]
            return result.text
        else:
            raise DocumentError(
                    (
                        '<Array key="{0}"> in line {1} '
                        'does not have <Entry>[{2}]'
                    ).format(node.get('key'), node.sourceline, n))

    @staticmethod
    def __findLeaves(node):
        groups = EtherLabMessages.__findNodeGroups(node)
        if not groups:
            return [node]

        result = []
        for n in groups:
            result.extend(EtherLabMessages.__findLeaves(n))

        return result

    def __visitLeaf(self, node, visitor):
        groups = EtherLabMessages.__findNodeGroups(node)
        if not groups and node.get('deprecated') != '1':
            try:
                count = int(node.get('width','1'))
            except ValueError as e:
                raise DocumentError(node,
                        (
                            'Error casting attribute width to int: {0}'
                        ).format(e))

            try:
                visitor.visitNodeGroup(self, node,
                        EtherLabMessages.__nodeGroupPath(node), count)
            except DocumentError as e:
                raise DocumentError(node, e)

        for n in groups:
            self.__visitLeaf(n, visitor)

    @staticmethod
    def __nodeGroupPath(node):
        parent = node.getparent()
        if parent is not None:
            return (EtherLabMessages.__nodeGroupPath(parent) + '/' +
                    (EtherLabMessages.__nodeGroupName(node) or ''))
        return ''

    @staticmethod
    def __nodeGroupName(node):
        name = node.find('Node')
        if name is not None and node.NodeGroup:
            return name.get('name')
        else:
            return node.get('name')

    @staticmethod
    def __findNodeGroup(node, name):
        for group in node.findall('NodeGroup'):
            n = group.find('Node')
            if n is not None and n.get('name') == name:
                return group
            if group.get('name') == name:
                return group
        return None

    @staticmethod
    def __findNodeGroups(node):
        return node.findall('NodeGroup')

    @staticmethod
    def __findText(node):
        return node.findall('Text')

    @staticmethod
    def __findArray(node):
        return node.findall('Array')

    @staticmethod
    def __findEntry(node):
        return node.findall('Entry')

    def __testNodeGroup(self, node):

        errors = []
        result = False

        if node.NodeGroup:
            name = self.__nodeGroupName(node)
            if not name:
                errors.append('<Name> element missing or empty')

        groups = self.__findNodeGroups(node)

        if len(groups):
            # Recurse into NodeGroups
            for n in groups:
                result |= self.__testNodeGroup(n)
        elif node.get('deprecated') == '1':
            errors.append('NodeGroup is deprecated')

        else:

            # Make sure attribute 'type' is correct
            types = ('Error','Warning','Information')
            if node.get('type') not in types:
                errors.append('type attribute not one of {}'.format(
                    ','.join(types)))

            # Make sure attribute 'width' is an integer
            widthStr = node.get('width')
            if widthStr is not None:
                try:
                    width = int(widthStr)
                except:
                    errors.append(
                            (
                                'attribute width="{}" is not a valid integer'
                            ).format(widthStr))
                    width = None
            else:
                width = 1

            # Make sure there are <Text> nodes
            text = EtherLabMessages.__findText(node)
            if not len(text):
                errors.append('Text missing')

            for t in text:
                # Walk though all <Map>'s in text
                for e in [n for n in t if n.tag == 'Map']:

                    key = e.get('key')
                    if not key:
                        errors.append('Attribute "key" for <Map> is missing')
                    else:
                        array = EtherLabMessages.findMap(node, key)
                        if array is not None:
                            count = len(array)
                            if width and count != width:
                                errors.append('Map key="{0}" has {1} elements, but width={2}'.format(
                                    key, count, width))
                        else:
                            errors.append('Map key="{}" is missing'.format(key))

            # Make sure there is a <Description>
            if node.find('Description') is None:
                errors.append('Description missing')


        if len(errors):
            path = self.__nodeGroupPath(node)
            print ("Errors for {} in line {}:".format(path, node.sourceline))
            for i in errors:
                print ("    - {}".format(i))

        return result or bool(len(errors))


    @staticmethod
    def __getLanguages(node):

        lang = set([z.get('lang')
            for x in EtherLabMessages.__findArray(node)
            for y in EtherLabMessages.__findEntry(x)
            for z in EtherLabMessages.__findText(y)])

        groups = EtherLabMessages.__findNodeGroups(node)
        if groups:
            for i in groups:
                lang.update(EtherLabMessages.__getLanguages(i))
        else:
            lang.update([x.get('lang') for x in EtherLabMessages.__findText(node)])

        lang.discard(None)

        return lang

    @staticmethod
    def __getLangElement(name, node, lang):
        default = None

        for t in node.findall(name):
            l = t.get('lang')

            if l is None:
                default = t
            elif l == lang:
                return t

        return default

######################################################################
class ExportPlain:
    def __init__(self, *argv):

        parser = argparse.ArgumentParser(
                description="Export a flattened version"
                )
        parser.add_argument('-l', '--lang', action='append',
                help="Languages to export (default: all)")
        parser.add_argument('file', nargs='?', default=sys.stdout,
                help="XML output file")
        parser.parse_args(argv, self)

        self.__root = etree.Element('EtherLabPlainMessages')

    def __del__(self):
        if self.lang:
            document = etree.ElementTree(self.__root)
            document.write(self.file,
                    encoding = 'UTF-8',
                    xml_declaration = True,
                    pretty_print = True)

    def visitNodeGroup(self, messages, group, path, width):
        if width > 1:
            fmt = '{0}/{1}'
        else:
            fmt = '{0}'

        if self.lang is None:
            self.lang = messages.languages()

            if not self.lang:
                self.lang = [None]

        text = {}
        description = {}
        for lang in self.lang:
            text[lang] = messages.getText(group, lang)
            description[lang] = '\n'.join([x
                for x in
                [x.strip()
                    for x in (messages.getDescription(group, lang) or "").split('\n')]
                if len(x)])

        for i in range(width):
            msgElem = etree.SubElement(self.__root, 'Message')
            msgElem.set('type', group.get('type'))
            msgElem.set('variable', fmt.format(path, i))

            textElem = etree.SubElement(msgElem, 'Text')
            descriptionElem = etree.SubElement(msgElem, 'Description')

            for lang in self.lang:
                elem = etree.SubElement(textElem, 'Translation')
                elem.set('lang', lang or '')
                elem.text = messages.expand(text[lang], lang, i)

                elem = etree.SubElement(descriptionElem, 'Translation')
                elem.set('lang', lang or '')
                elem.text = description[lang]

######################################################################
class ExportMatlab:
    def __init__(self, *argv):

        parser = argparse.ArgumentParser(
                description="Export a flattened version"
                )
        parser.add_argument('-l', '--lang', default='en',
                help="Languages to export (default: en)")
        parser.add_argument('file', nargs='?', default=sys.stdout,
                type=CodecOpen('w'),
                help="Matlab m-file")
        parser.parse_args(argv, self)

        self.file.write(
"""function str = message(path)

str = [];

if nargin < 1
    path = gcb;
end

if ~isstr(path)
    return
end

[s,p] = strtok(path,'/');
if strcmp(s,strtok(gcs,'/'))
    path = p;
end

messages = {...
"""
)

    def __del__(self):
        self.file.write(
"""};

try
    str = messages{strcmp(messages(:,1), path), 2};
catch
    disp([path ' does not have a message'])
end
"""
)

    def visitNodeGroup(self, messages, group, path, width):
        if width > 1:
            fmt = "'{0}', {{{1}}};\n"
        else:
            fmt = "'{0}', {1};\n"

        if self.lang is None:
            self.lang = messages.languages()
            if len(self.lang):
                self.lang = self.lang.pop()

        textNode = messages.getText(group, self.lang)
        text = []

        for i in range(width):
            s = messages.expand(textNode, self.lang, i)
            s = " ".join([s.strip() for s in s.split('\n')])
            text.append("'{}'".format(s.strip()))

        self.file.write(fmt.format(path, ",".join(text)))


######################################################################
class ExportLaTeX:
    __colorMap = {
            'Information': 'blue',
            'Warning': 'orange',
            'Error': 'red'
            }

    def __init__(self, *argv):
        parser = argparse.ArgumentParser(
                description="Export LaTeX file of messages and descriptions"
                )
        parser.add_argument('-l', '--lang')
        parser.add_argument('file', nargs='?', default=sys.stdout,
                type=CodecOpen('w'),
                help="LaTeX output file"
                )
        parser.parse_args(argv, self)

    def visitNodeGroup(self, messages, group, path, width):

        if self.lang is None:
            self.lang = messages.languages()
            if len(self.lang):
                self.lang = self.lang.pop()

        text = messages.getText(group, self.lang)
        description = ' '.join([x.strip()
            for x in (messages.getDescription(group, self.lang) or '').split('\n')])

        message = text.text or ''
        for n in text:
            if n.tag == 'Map':
                # Compress a <Map> to a comma separated list
                array = messages.findMap(group, n)
                message += ('\\textit{'
                        + ', '.join([messages.getMapText(array, self.lang, i)
                            for i in range(width)])
                        + '}')
            elif n.tag == 'Index':
                # Replace <Index> with a range first...last
                offset = int(n.get('offset', '1'))
                if width > 1:
                    message += '{0}\\ldots {1}'.format(offset, offset+width-1)
                else:
                    message += str(offset)

            message += n.tail or ''

        self.file.write(u'{{\\bf \\textcolor{{{0}}}{{{1}}}}} {2}\n\n'.format(
                self.__colorMap[group.get('type')], message, description))

######################################################################
class ExportQtHeader:
    """
    Create a header file with lots of QT_TRANSLATE_NOOP()'s

    This file is used by lupdate to create a list of linguist *.ts
    files, where the error message paths are included. Thereafter ExportQt
    is applied on the *.ts files to update the translation.
    """

    def __init__(self, *argv):

        parser = argparse.ArgumentParser(
                description=(
                    "Export a header file with QT_TRANSLATE_NOOP()'s. "
                    "If this file is added to the list of HEADERS in the "
                    "project file, Qt's lupdate will use it to generate "
                    "the translation TS files."))
        parser.add_argument('-n', '--namespace', default='Pd::MessageModel',
                help='Namespace for QObject::tr() lookup (default: %(default)s)')
        parser.add_argument('file', nargs='?', default=sys.stdout,
                type=CodecOpen('w'),
                help='header file to export')
        parser.parse_args(argv, self)

        self.file.write("// Messages generated by {0}\n\n".format(sys.argv[0]))

    def visitNodeGroup(self, messages, group, path, width):
        if width > 1:
            fmt = u'QT_TRANSLATE_NOOP("{0}","{1}[{2}]")\n'
        else:
            fmt = u'QT_TRANSLATE_NOOP("{0}","{1}")\n'

        for i in range(width):
            self.file.write(fmt.format(self.namespace, path, i))

######################################################################
class TS:
    """
    Update Qt linguist *.ts files

    This will _not_ create any nodes. This has to be done using
    ExportQtHeader first, then running lupdate and subsequently
    running ExportQt on every .ts file
    """

    __lang = None

    def __init__(self, ns, file):
        self.namespace = ns
        self.file = file

        # Read the xml file
        self.__xml = etree.parse(self.file)
        self.__root = self.__xml.getroot()

        # Make sure nothing has changed
        if self.__root.tag != 'TS':
            raise Exception("{0} does not have <TS> root element".format(
                self.file))

        self.__ts = self.__root.get('language')

        # Find <context> that has the namespace
        for ctxt in self.__root.findall('context'):
            if self.namespace == ctxt.find('name').text:
                break
        else:
            raise Exception("{0} does not have namespace {1}".format(
                self.file, self.namespace))

        # Run through all <message>'s to create a dictionary with the
        # path as key. Makes it easy to find later
        self.__message = dict((x.find('source').text, x)
                for x in ctxt.findall('message'))

    def __del__(self):
        # Save file, only if __lang is set
        if self.__lang:
            self.__xml.write(self.file,
                    encoding = 'UTF-8',
                    xml_declaration = True)

    def visitNodeGroup(self, messages, group, path, width):

        if not self.__lang:
            # Set __lang as one of the languages in messages that match
            # the language attribute in the root <TS> node
            lang = messages.languages()
            if not lang:
                self.__lang = [None]
            else:
                for i in lang:
                    if self.__ts.startswith(i):
                        self.__lang = i
                        break
                else:
                    raise DocumentError((
                        "Language {0} is not available"
                        ).format(self.__ts))

        # Path format specifier; append [n] at end of path
        if width > 1:
            fmt = '{0}[{1}]'
        else:
            fmt = '{0}'

        for i in range(width):
            p = fmt.format(path, i)

            if self.__message.has_key(p):
                msg = self.__message[p]

                # Set text
                translation = msg.find('translation')
                translation.text = messages.expand(
                        messages.getText(group, self.__lang), self.__lang, i)

                # Remove all attributes from <translation>
                for i in translation.attrib:
                    translation.attrib.pop(i)

                # Remove all elements from <translation>
                for i in translation:
                    translation.remove(i)

                # Remove <oldsource> from <message>
                oldsrc = msg.find('oldsource')
                if oldsrc is not None:
                    oldsrc.getparent().remove(oldsrc)
            else:
                print("Path {0} was not found. Run lupdate".format(p),
                        file=sys.stderr)

class ExportQt:
    """
    Update Qt linguist *.ts files

    This will _not_ create any nodes. This has to be done using
    ExportQtHeader first, then running lupdate and subsequently
    running ExportQt on every .ts file
    """

    __lang = None

    def __init__(self, *argv):
        parser = argparse.ArgumentParser(
                description=(
                    "Insert the messages into Qt linguists TS files. "
                    "Run lupdate *.pro before issuing this command"
                    ))
        parser.add_argument('-n', '--namespace', default='Pd::MessageModel',
                help='Namespace for QObject::tr() lookup (default: %(default)s)')
        parser.add_argument('file', nargs='+',
                help='Qt linguist XML translation file (*.ts)')
        parser.parse_args(argv, self)

        self.__doc = [TS(self.namespace, f) for f in self.file]

    def visitNodeGroup(self, messages, group, path, width):

        for d in self.__doc:
            d.visitNodeGroup(messages, group, path, width)


######################################################################
######################################################################
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print ((
            "Usage:\n"
            "\t{0} messagefile command <options>\n"
            "where:\n"
            "  - messagefile: XML EtherLab messages\n"
            "  - command: one of import|test|plain|latex|qtheader|qt|matlab\n"
            "  - options: command options; see -h on command"
            ).format(sys.argv[0]))
        sys.exit(0)

    messages = EtherLabMessages(sys.argv[1])

    if sys.argv[2].lower() == 'import':
        messages.importTextFile(*sys.argv[3:])
    elif sys.argv[2].lower() == 'test':
        messages.test()
    elif sys.argv[2].lower() == 'plain':
        messages.export(ExportPlain(*sys.argv[3:]))
    elif sys.argv[2].lower() == 'latex':
        messages.export(ExportLaTeX(*sys.argv[3:]))
    elif sys.argv[2].lower() == 'qtheader':
        messages.export(ExportQtHeader(*sys.argv[3:]))
    elif sys.argv[2].lower() == 'qt':
        messages.export(ExportQt(*sys.argv[3:]))
    elif sys.argv[2].lower() == 'matlab':
        messages.export(ExportMatlab(*sys.argv[3:]))
