<template>
  <el-upload
    class="upload"
    drag
    action="/upload"
    multiple
    :auto-upload="false"
    :data="info"
    :on-change="md5File"
    ref="myFile"
  >
    <i class="el-icon-upload"></i>
    <div class="el-upload__text">
      Drag files here
      <em>or Click to Upload</em>
    </div>
    <div class="el-upload__tip" slot="tip">file size less than 128Mb</div>
  </el-upload>
</template>

<script>
import SparkMD5 from "spark-md5";

export default {
  name: "Upload",
  data() {
    return {
      info: { fileInfo: "" },
    };
  },

  methods: {
    //   beforeUpload(file) {
    //     var sizeok = file.size / 1024 / 1024 < 128;
    //     if (sizeok) {
    //       var freader = new FileReader();
    //       var spark = new SparkMD5.ArrayBuffer();
    //       var md5;
    //       var user = window.sessionStorage["user"];
    //       var size = file.size;
    //       var filename = file.name;
    //       freader.readAsArrayBuffer(file);
    //       return new Promise((resolve, reject) => {
    //         freader.onload = function (e) {
    //           spark.append(e.target.result);
    //           md5 = spark.end();
    //           if (user && md5 && size && filename) {
    //             this.info = [user, filename, md5, size].join("|");
    //             console.log(this.info);
    //             resolve(e.target.result);
    //           } else {
    //             alert("something wrong...");
    //             reject(false);
    //           }
    //         };
    //       });
    //     } else {
    //       alert("file size exceed 128Mb");
    //       return false;
    //     }
    //   },
    upload: function (obj) {
      console.log(this.info);
      this.$refs[obj].submit();
    },
    md5File(file) {
      const _this = this;

      var sizeok = file.size / 1024 / 1024 < 128;
      if (sizeok) {
        var freader = new FileReader();
        var spark = new SparkMD5.ArrayBuffer();
        var md5;
        var user = window.sessionStorage["user"];
        var token = window.sessionStorage["token"];
        var size = file.size;
        var filename = file.name;

        freader.readAsArrayBuffer(file.raw);

        freader.onload = function (e) {
          spark.append(e.target.result);
          md5 = spark.end();
          if (user && md5 && size && filename) {
            _this.info.fileInfo = [user, filename, md5, size].join("|");
          } else {
            alert("something wrong...");
          }

          //check md5 first
          _this
            .axios({
              method: "post",
              url: "/md5",
              data: {
                user: user,
                token: token,
                filename: filename,
                md5: md5,
              },
            })
            .then((res) => {
              if (res.status == 200) {
                if (res.data.code == "004" || res.data.code == "005") {
                  //md5 success
                } else {
                  console.log("md5 failed, uploading...");
                  _this.upload("myFile");
                }
              } else {
                alert("upload failed");
              }
            });
        };
      } else {
        alert("file size exceeds 125Mb");
      }
    },
  },
};
</script>