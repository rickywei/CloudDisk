<template>
  <div id="loginForm">
    <el-form :label-position="labelPosition" label-width="80px" :model="loginData">
      <el-form-item label="user">
        <el-input v-model="loginData.user"></el-input>
      </el-form-item>
      <el-form-item label="pwd">
        <el-input v-model="loginData.pwd"></el-input>
      </el-form-item>
    </el-form>
    <el-button type="success" round @click="register">register</el-button>
    <el-button type="success" round @click="login">login</el-button>
  </div>
</template>

<script>
export default {
  name: "LoginPage",
  data() {
    return {
      labelPosition: "left",
      loginData: {
        user: "",
        pwd: "",
      },
    };
  },
  methods: {
    register() {
      this.$router.push({ path: "/RegisterPage" });
    },
    login() {
      // console.log("login...", this.loginData.user, this.loginData.passwd);
      this.axios({
        method: "post",
        url: "/login",
        data: {
          user: this.loginData.user,
          pwd: this.loginData.pwd,
        },
      }).then((res) => {
        if (res.status == 200) {
          if (res.data.code == "000") {
            window.sessionStorage["user"] = this.loginData.user;
            window.sessionStorage["token"] = res.data.token;

            this.$router.push({ path: "/FilesPage" });
          } else {
            alert("login failed, please check your account and passewd");
          }
        } else {
          alert("login failed");
        }
      });
    },
  },
};
</script>
