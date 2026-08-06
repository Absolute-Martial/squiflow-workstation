import QtQuick
import QtQuick.Controls
Control{id:root;required property string stableFileId;property string accessibleName:qsTr("File preview");contentItem:Image{source:"image://squiflow-files/"+encodeURIComponent(root.stableFileId);sourceSize.width:256;sourceSize.height:256;fillMode:Image.PreserveAspectFit;asynchronous:true;cache:false;Accessible.name:root.accessibleName;Accessible.role:Accessible.Graphic}}
