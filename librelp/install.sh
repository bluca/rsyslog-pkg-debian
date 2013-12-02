!#/bin/sh
TARGZFILES=`ls *.dsc`
PLATFORM="squeeze wheezy"
ARCHTECT="amd64 i386"
BRANCHES="v7-stable v7-devel"

echo "-------------------------------------"
echo "--- RPMMaker                      ---"
echo "-------------------------------------"

echo "Select DSC Filebasename:"
select szDscFile in $TARGZFILES
do
        break;
done

echo "Select Ubuntu DIST:"
select szPlatform in $PLATFORM
do
        break;
done

echo "Select CPU Platform:"
select szArch in $ARCHTECT "All"
do
        case $szArch in "All")
                szArch=$ARCHTECT;
        esac
        break;
done

echo "Select RSyslog BRANCH:"
select szBranch in $BRANCHES
do
        echo "Installing Packages for '$szDscFile' on '$szPlatform'/'$szArch' into ($szBranch)"
        break;
done

# Loop through architects
for szArchitect in $szArch;
do
        szAddArch="";
        if [ $szArchitect = "i386" ]; then
                szAddArch="-i386";
        fi

        # Set Basename!
        szDscFileBase=`basename $szDscFile .dsc`

        #pbuilder-dist precise create
        APPENDSUFFIX="_$szArchitect.changes"
	# only install        pbuilder-dist $szPlatform $szArchitect build $szDscFile
        dput $szBranch /root/pbuilder/$szPlatform$szAddArch\_result/$szDscFileBase$APPENDSUFFIX
done;

