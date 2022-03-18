function onOpen() {
  var ui = SpreadsheetApp.getUi();
  ui.createMenu('Export')
    .addItem('Export to json', 'exportToJson')
    .addToUi();
}

function exportToJson() {

  var sheet = SpreadsheetApp.getActiveSheet();

  const colsNum = 7;
  // Omit first row and last column
  var data = sheet.getRange(
    2, 1,
    sheet.getLastRow() - 1,
    colsNum).
    getValues();

  var jsonText = JSON.stringify(
    getExportObject(transformFields(data)),
    null, 2);

  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var defaultFileName = ss.getName().replace(/ /g, '_') + '.json';

  var html = HtmlService.createHtmlOutput(
    dialogHtmlContent(defaultFileName, jsonText))
    .setWidth(405)
    .setHeight(560);

  SpreadsheetApp.getUi()
    .showModalDialog(html, 'Export to json');

}

function dialogHtmlContent(defaultFileName, jsonText) {
  return '<!DOCTYPE html> \
    <html>\
     <head>\
      <base target="_top">\
     </head>\
     <body>\
      <form onsubmit="download(this[\'text\'].value)">\
       <textarea readonly rows="30" cols="50" style="resize:none" name="text">' +
    jsonText +
    '</textarea></br>\
       <div style="text-align:right;padding:10px">\
        <input type="submit" style="padding:10px" value="Save...">\
       </div>\
      </form>\
      <script>\
       function download(text) {\
         var element = document.createElement("a");\
         element.setAttribute("href", "data:text/plain;charset=utf-8," + encodeURIComponent(text));\
         element.setAttribute("download", "' + defaultFileName + '");\
         element.style.display = "none";\
         document.body.appendChild(element);\
         element.click();\
         document.body.removeChild(element);\
       }\
      </script>\
     </body>\
    </html>';
}

const fieldsTransformMap = {
  'n/a'              : '',
  '(out-scope)'      : '',
  'VR_VDD/HVDD/DVDD' : 'VR'
};

function transformFields(table) {
  return table.map(
    (row) => row.map(
      (field) => {
        if (field in fieldsTransformMap) {
          return fieldsTransformMap[field];
        } else {
          return field;
        }
      }));
}

function getExportObject(data) {
  var res = {};
  for (var i = 0; i < data.length; i++) {
    if (data[i][0] != "") {
      res[data[i][0]] = {
        association      : getDeviceAssociation(data, i),
        power_rail       : [ ],
        erot_control     : [ ],
        pin_status       : [ ],
        interface_status : [ ],
        protocol_status  : [ ],
        firmware_status  : [ ]
      };
    }
  }
  return res;
}

function getDeviceAssociation(data, rowIndx) {
  // Array Sum across whole row except for "this" column
  // and ignoring empty fields
  return data[rowIndx].slice(1).
    filter((elem) => elem != "");
}
