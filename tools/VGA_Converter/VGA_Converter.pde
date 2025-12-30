
import controlP5.*;
import java.util.Arrays;
import java.util.*;

ControlP5 cp5;
Textfield textField1, textField2;
ScrollableList dropdown;
CheckBox checkBox;
PApplet secondWindow;

//win size
int winX = 400;
int winY = 200;

// Кнопка
boolean buttonPressed = false;
int buttonX = 5, buttonY = 7, buttonW = 100, buttonH = 17;

//Format selected
int format = 0;
String formatS;
int imgCont = 0;
int pos = 0;
int pos1, pos2, pos3, pos4;

//Check box
boolean allImage = false;
boolean createHead = false;

// Файлы
File[] files;
String myFileName = "", myfullFileName = "";
String fileName, filePath, fullFileName, fileOnlyName;
String[] supportedExtensions = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".wbmp", ".JPG", ".JPEG", ".PNG", ".GIF", ".BMP", ".TIF", ".TIF", ".TIFF", ".WBMP"};

//picture
PImage img = null, copyImg = null;
//String imgPath = "";

//Arrays
ArrayList<String> head = new ArrayList<String>();
ArrayList<String> posData = new ArrayList<String>();
ArrayList<String> data = new ArrayList<String>();
ArrayList<String> allData = new ArrayList<String>();
String line = "";

int MaskCol = 0;

void setup(){
    pixelDensity(1);  // отключает HiDPI
    size(400, 200);

    // Вычисляем координаты для центрирования окна
    int x = (displayWidth - width) / 2;
    int y = (displayHeight - height) / 2;
    
    // Устанавливаем позицию окна
    surface.setLocation(x, y);
  
    cp5 = new ControlP5(this);
    createUI();  // создаём элементы интерфейса
}  

void createUI(){
    // Текстовое поле
    textField1 = cp5.addTextfield("fileNameField1")
        .setPosition(270, 5)
        .setSize(30, 22)
        .setColorBackground(color(255))
        .setAutoClear(false)
        .setColor(color(0));

    textField1.setLabelVisible(false);
    textField1.getCaptionLabel().setText(""); // <- полностью убираем подпись

    textField2 = cp5.addTextfield("fileNameField2")
        .setPosition(340, 5)
        .setSize(50, 22)
        .setColorBackground(color(255))
        .setAutoClear(false)
        .setColor(color(0));

    textField2.setLabelVisible(false);
    textField2.getCaptionLabel().setText(""); // <- полностью убираем подпись

    // Выпадающий список
    List<String> l = Arrays.asList("RGB332", "RGB565");
    dropdown = cp5.addScrollableList("dropdown")
        .setPosition(113, 8)
        .setSize(50, 140)
        .setBarHeight(15)
        .setItemHeight(15)
        .addItems(l);
        dropdown.setValue(0);  // <- отдельно после присвоения переменной RGB332

    // Флажки
    checkBox = cp5.addCheckBox("options")
        .setPosition(170, 11)
        .setSize(10, 10)
        .addItem("Опция 1", 0)
        //.addItem("Опция 2", 1)
        .setWidth(100)
        .setHeight(30);

    // Изменение текста и цвета подписей
    for (Toggle t : checkBox.getItems()) {
        t.getCaptionLabel().setColor(color(0, 0, 0));  
    }
    checkBox.getItem(0).getCaptionLabel().setText("All img");
    //checkBox.getItem(1).getCaptionLabel().setText("With Palette");    
}

void draw(){
    background(240);
        
    // Рисуем кнопку
    fill(buttonPressed ? color(150) : color(200));
    rect(buttonX, buttonY, buttonW, buttonH, 10);  
    fill(0);
    textAlign(CENTER, CENTER);
    textSize(12);
    text("Открыть файл", buttonX + buttonW / 2, buttonY + buttonH / 2);
    text("MaskCol", 240, 15);
    text("Name", 320, 15);

    stroke(0);
    strokeWeight(1);
    line(0, 0, 400, 0);
    line(0, 30, 400, 30);

    if (img != null) image(img, 0, 31);
}

PImage smooth3x3(PImage src){
  PImage dst = createImage(src.width, src.height, RGB);
  src.loadPixels();
  dst.loadPixels();

  for (int y = 0; y < src.height; y++){
    for (int x = 0; x < src.width; x++){
      int rs=0, gs=0, bs=0, cnt=0;

      for (int dy=-1; dy<=1; dy++){
        int yy = y + dy;
        if (yy < 0 || yy >= src.height) continue;
        for (int dx=-1; dx<=1; dx++){
          int xx = x + dx;
          if (xx < 0 || xx >= src.width) continue;
          int c = src.pixels[yy*src.width + xx];
          rs += (c >> 16) & 255;
          gs += (c >> 8) & 255;
          bs += c & 255;
          cnt++;
        }
      }

      rs /= cnt; gs /= cnt; bs /= cnt;
      dst.pixels[y*src.width + x] = color(rs, gs, bs);
    }
  }

  dst.updatePixels();
  return dst;
}

//===Event===
void mousePressed() {
    if (mouseX >= buttonX && mouseX <= buttonX + buttonW &&
        mouseY >= buttonY && mouseY <= buttonY + buttonH) {
        buttonPressed = true;

        // Получаем текст из Textfield только при нажатии кнопки
        MaskCol = getIntFromTextfield(textField1, 0x1C); //Mask color for DEFAULT!!!
        println("maskcol: " + MaskCol);

        myFileName = textField2.getText();
        myfullFileName = myFileName + ".h";
        println("Color: " + formatS + ", All image: " + allImage + ", Name: " + myFileName + ", File name: " + myfullFileName);

        selectInput("Выберите файл:", "fileSelected");
    }
}

void mouseReleased() {
  buttonPressed = false;
}

void dropdown(int n) {
  format = n;
  formatS = dropdown.getItem(n).get("name").toString();
  println("vibrano" + n);
  dropdown.close();
}

void controlEvent(ControlEvent e) {
    if (e.isFrom(checkBox)) {
        allImage = checkBox.getItem(0).getState();
    }

    //if (e.isFrom(textField1)) {

    //}
}

void fileSelected(File file) {
    if (file != null) {
        // Получаем папку
        filePath = file.getParent().trim();
        fullFileName = file.getAbsolutePath(); 

        // Массив всех файлов в папке
        File folder = new File(filePath);
        File[] files = folder.listFiles();

        if (files != null) {
            if (myFileName.equals("")) {
                myFileName = file.getName().substring(0, file.getName().lastIndexOf('.'));
                myFileName = myFileName.trim();  // ← теперь работает правильно!
                myfullFileName = myFileName + ".h";
            }
            
            if (allImage) {
                // Обрабатываем все файлы
                for (File f : files) {
                    if (f.isFile()) {  
                        processFile(f);
                    }                    
                }
            } else {
                // Обрабатываем только выбранный файл
                processFile(file);
            }
        } else {
            println("Папка пуста или недоступна!");
        }

        createHead();
        if (createHead){ 
            line = "";
            posData.add(line);
            saveFile();
        }    
    }
}

// Вынесем повторяющийся код обработки файла в отдельную функцию
void processFile(File f) {
    String _filePath = f.getAbsolutePath();

    // Проверяем расширение файла
    boolean isValidExtension = false;
    for (String ext : supportedExtensions) {
        if (_filePath.toLowerCase().endsWith(ext.toLowerCase())) {
            isValidExtension = true;
            break;
        }
    }

    if (isValidExtension) {
        System.gc(); // не гарантировано, но иногда помогает
        img = null; 
        img = loadImage(_filePath); 
        noStroke();  // Отключаем контуры для изображения
        if (img != null) {
            copyImg = null;
            copyImg = img.get();
            println("Файл: " + fullFileName);
            println("Ширина изображения: " + img.width);
            println("Высота изображения: " + img.height); 
            convertImg();
            winResize();  
            createSpriteData();

            createHead = true;
            createPos();
            imgCont++;
        }    
    }
}

void winResize(){
    if ((winX < img.width) || (winY < img.height + 5)) 
    {
        winX = img.width + 5;
        winY = img.height + 5;
    }  else {
        winX = 400;
        winY = 200;
    }
    
    surface.setSize(winX, winY);
}

public class SecondWindow extends PApplet {
    public void settings() {
        size(500, 500);
    }

    public void setup() {
        background(255);
    }

    public void draw() {
        background(255); // Очистка белым цветом (можно 0 — чёрный)
        //fill(0);
        //if (mousePressed) {
        //  ellipse(mouseX, mouseY, 10, 10); // рисование по нажатию
        //}
        if (copyImg != null) image(copyImg, 0, 0, copyImg.width * 2, copyImg.height * 2);
    }
}

int getIntFromTextfield(Textfield tf, int defaultValue) {
    String text = tf.getText();
    if (text == null || text.equals("")) {
        return defaultValue; // если поле пустое, возвращаем значение по умолчанию
    }
    
    text = text.trim(); // убираем пробелы по краям
    
    try {
        if (text.startsWith("0x") || text.startsWith("0X")) {
            // HEX формат, например: 0xFF00FF
            return (int) Long.parseLong(text.substring(2), 16);
        } else {
            // Десятичное число
            return Integer.parseInt(text);
        }
    } catch (NumberFormatException e) {
        println("Ошибка: введено не корректное число! Значение по умолчанию: " + defaultValue);
        return defaultValue;
    }
}

//------------------------------------------------------------------------------------
void convertImg(){
    //img = smooth3x3(img);
    int col, a, r, g, b, aa, rr, gg, bb;
            
    for (int y = 0; y < img.height; y++){
        for (int x = 0; x < img.width; x++){
          col = img.get(x, y);
          a = (int)alpha(col);
          r = (int)red(col);
          g = (int)green(col);
          b = (int)blue(col);
          if (a > 0 && a < 255) println("semi alpha at", x, y, "a=", a);

          
          // порог для антиалиаса (иначе "грязные" пиксели по краям)
          boolean transparent = (a < 10);
          
          if (format == 0) { // RGB332
              rr = (r & 0xFF) >> 5; // 3 бита
              gg = (g & 0xFF) >> 5; // 3 бита
              bb = (b & 0xFF) >> 6; // 2 бита
          
              // 8-bit packed
              int c8 = transparent ? (MaskCol & 0xFF) : ((rr << 5) | (gg << 2) | bb);
          
              // правильное восстановление в RGB888 для preview
              int r3 = (c8 >> 5) & 0x07;
              int g3 = (c8 >> 2) & 0x07;
              int b2 =  c8       & 0x03;
          
              int r8 = (r3 << 5) | (r3 << 2) | (r3 >> 1);
              int g8 = (g3 << 5) | (g3 << 2) | (g3 >> 1);
              int b8 = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
          
              copyImg.set(x, y, color(r8, g8, b8));
          
          } else { // RGB565
              rr = (r & 0xFF) >> 3; // 5 бит
              gg = (g & 0xFF) >> 2; // 6 бит
              bb = (b & 0xFF) >> 3; // 5 бит
          
              // 16-bit packed
              int c16 = transparent ? (MaskCol & 0xFFFF) : ((rr << 11) | (gg << 5) | bb);
          
              // правильное восстановление в RGB888 для preview
              int r5 = (c16 >> 11) & 0x1F;
              int g6 = (c16 >> 5)  & 0x3F;
              int b5 =  c16        & 0x1F;
          
              int r8 = (r5 << 3) | (r5 >> 2);
              int g8 = (g6 << 2) | (g6 >> 4);
              int b8 = (b5 << 3) | (b5 >> 2);
          
              copyImg.set(x, y, color(r8, g8, b8));
           }   
        }     
    }

    if (secondWindow == null) {
        println("Открытие второго окна...");
        String[] args = {"SecondWindow"};
        secondWindow = new SecondWindow(); // создаём экземпляр
        PApplet.runSketch(args, (PApplet)secondWindow);
    } 
}

//====Make file====
void saveFile(){
    if (imgCont > 255){
        println("Too many images max 256...");
        return;
    }

    allData.addAll(head);
    allData.addAll(posData);
    allData.addAll(data);
    line = "};";
    allData.add(line);
    saveStrings(filePath + "\\" + myfullFileName, allData.toArray(new String[0]));
}

void createHead(){
    line = "#pragma once\n"; head.add(line);
    
    String tmp; 
    if (format == 0){
        tmp = "0x08";
    } else {
        tmp = "0x10";
    }  
    
    //Save sprite
    line = "const uint8_t _" + myFileName + "[] PROGMEM = {"; head.add(line);

    line = "    " + tmp + ", 0x" + hex((imgCont - 1), 2) + ", "; 
    line += "// " + ((tmp.equals("0x08")) ? "8 bit RGB332" : "16 bit RGB565");
    line += ", images: " + imgCont + "\n";
    head.add(line);
}

void createPos(){
    pos1 = pos & 0xFF;
    pos2 = (pos >> 8) & 0xFF;
    pos3 = (pos >> 16) & 0xFF;
    pos4 = (pos >> 24) & 0xFF;

    line = "    ";
    line += "0x" + hex(pos1, 2) + ", " +
            "0x" + hex(pos2, 2) + ", " +
            "0x" + hex(pos3, 2) + ", " +
            "0x" + hex(pos4, 2) + ", // " + imgCont + ": " + pos + " - " + img.width + "x" + img.height;
    posData.add(line);  
    
    pos += img.width * img.height * ((format == 0) ? 1 : 2) + 4;    
}

void createSpriteData(){
    int hi, lo, size;

    if (img != null){
        //Width
        lo = (img.width >> 8) & 0xFF; hi = img.width & 0xFF;
        line = "    0x" + hex(hi, 2) + ", 0x" + hex(lo, 2)+ ", ";

        //Height
        lo = (img.height >> 8) & 0xFF; hi = img.height & 0xFF;
        line += "0x" + hex(hi, 2) + ", 0x" + hex(lo, 2)+ ", ";
        
        //Size
        size = img.width * img.height * ((format == 0) ? 1 : 2);
        line += "//Image size " + img.width + " x " + img.height + ", Data: " + size + " bytes";
        data.add(line);

        for (int y = 0; y < img.height; y++){
            line = "    ";

            for (int x = 0; x < img.width; x++){
                int c = img.get(x, y);
                int a = (int) alpha(c);  // вот тут альфа-канал (0 — полностью прозрачно, 255 — полностью непрозрачно)
                int r = (int) red(c);
                int g = (int) green(c);
                int b = (int) blue(c);

                if (format == 0){
                    int col = (a == 0) ? 0x1C : getColor(r, g, b);
                    line += "0x" + hex(col, 2) + ", ";
                } else {
                    int col = (a == 0) ? 0x7E0 : getColor(r, g, b);
                    lo = (col >> 8) & 0xFF; hi = col & 0xFF;
                    line += "0x" + hex(hi, 2) + ", 0x" + hex(lo, 2) + ", ";
                } 
            }

            data.add(line);
        }  
        
        line = "";
        data.add(line);
    }
}

public int getColor(int r, int g, int b) {
    int rr, gg, bb;
    
    switch (format){
        case 0:  //RGB332
            rr = (r & 0xFF) >> 5; // 3 бита
            gg = (g & 0xFF) >> 5; // 3 бита
            bb = (b & 0xFF) >> 6; // 2 бита
        return (rr << 5) | (gg << 2) | bb;  // 8 бит: RRR GGG BB
        
        case 1:  //RGB565
            rr = (r & 0xFF) >> 3;
            gg = (g & 0xFF) >> 2;
            bb = (b & 0xFF) >> 3;
        return (rr << 11) | (gg << 5) | bb;  // 16 бит: RRRRR GGGGG BBBBB 
    }

    return 0;
}    
