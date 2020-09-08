<template>
  <div id="registerForm">
    <el-form :label-position="labelPosition" label-width="80px" :model="registerData">
      <el-form-item label="user">
        <el-input v-model="registerData.user"></el-input>
      </el-form-item>
      <el-form-item label="nickname">
        <el-input v-model="registerData.nickname"></el-input>
      </el-form-item>
      <el-form-item label="passwd">
        <el-input v-model="registerData.passwd"></el-input>
      </el-form-item>
      <el-form-item label="phone">
        <el-input v-model="registerData.phone"></el-input>
      </el-form-item>
      <el-form-item label="email">
        <el-input v-model="registerData.email"></el-input>
      </el-form-item>
    </el-form>
    <el-button type="success" round @click="register">Register</el-button>
  </div>
</template>

<script>
export default {
  name: "RegisterPage",
  data() {
    return {
      labelPosition: "left",
      registerData: {
        user: "",
        nickname: "",
        passwd: "",
        phone: "",
        email: "",
      },
    };
  },
  methods: {
    register() {
      // console.log("register...", this.registerData.user, this.registerData.passwd);
      this.axios({
        method: "post",
        url: "/register",
        data: {
          userName: this.registerData.user,
          nickName: this.registerData.nickname,
          firstPwd: this.registerData.passwd,
          phone: this.registerData.phone,
          email: this.registerData.email,
        },
      }).then((res) => {
        if (res.status == 200) {
          if (res.data.code == "002") {
            this.$router.push({ path: "/loginPage" });
          } else {
            alert("register failed, please check your account and passewd");
          }
        } else {
          alert("register failed");
        }
      });
    },
  },
};
</script>
